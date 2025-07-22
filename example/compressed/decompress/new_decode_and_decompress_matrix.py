import os
import re
import glob
import leb128
import argparse
import math
from collections import defaultdict

def letter_to_number(s):
    """
    将字母标签转换为对应的数字ID（与Rust函数的number_to_letter的逆过程）
    """
    if not s:
        return 0
    
    # 单字母处理
    if len(s) == 1:
        return ord(s) - ord('a')
    
    # 多字母处理
    num = 0
    for i, char in enumerate(s[::-1]):
        # 每位字母对应的数值 (a=0, b=1, ... z=25)
        char_value = ord(char) - ord('a')
        num += char_value * (26 ** i)
    
    # 调整26进制映射偏移
    base_value = (26 ** len(s) - 26) // 25
    return base_value + num

# 从bin文件中读取数据
def read_bin_file(file_path, is_delta_file=False):
    """
    从bin文件中读取数据并解码
    :param file_path: bin文件路径
    :param is_delta_file: 是否为Delta优化文件
    :return: (占位符位置数, 实例数, 数据列表)
    """
    with open(file_path, 'rb') as f:
        # 读取占位符位置数（行数）和实例数（列数）
        num_placeholder_positions = leb128.u.decode_reader(f)[0]
        num_instances = leb128.u.decode_reader(f)[0]
        
        # Delta优化文件只有一个占位符位置
        if is_delta_file and num_placeholder_positions != 1:
            print(f"警告: Delta优化文件 {os.path.basename(file_path)} 的行数不是1 ({num_placeholder_positions})")
            return num_placeholder_positions, num_instances, None
        
        # 读取每个占位符位置的数据
        matrix = []
        for _ in range(num_placeholder_positions):
            # 读取编码标记（仅非Delta文件）
            if not is_delta_file:
                flag = leb128.u.decode_reader(f)[0]
            
            if is_delta_file or flag == 0:  # 原始值
                row_data = []
                for _ in range(num_instances):
                    value = leb128.u.decode_reader(f)[0]
                    row_data.append(value)
                matrix.append(row_data)
            elif not is_delta_file and flag == 1:  # Delta编码
                # 读取基准值
                base_value = leb128.u.decode_reader(f)[0]
                row_data = [base_value]
                
                # 读取并解码delta值
                for _ in range(1, num_instances):
                    encoded_delta = leb128.i.decode_reader(f)[0]
                    row_data.append(encoded_delta)
                
                # 重建原始序列
                decoded_row = decode_delta_sequence(row_data)
                matrix.append(decoded_row)
            else:
                print(f"错误: 文件 {os.path.basename(file_path)} 未知的编码标记: {flag}")
                return num_placeholder_positions, num_instances, None
        
        return num_placeholder_positions, num_instances, matrix

# 从Delta序列重建原始值序列
def decode_delta_sequence(deltas):
    """从Delta序列重建原始值序列"""
    if not deltas:
        return []
    
    values = [deltas[0]]
    for i in range(1, len(deltas)):
        values.append(values[-1] + deltas[i])
    
    return values

# 处理Delta优化文件
def process_delta_file(file_path, length_specs):
    """
    处理Delta优化格式的bin文件
    :param file_path: 文件路径
    :param length_specs: 长度规格列表
    :return: 实例数据列表
    """
    # 读取原始delta编码数据
    num_positions, num_instances, encoded_data = read_bin_file(file_path, is_delta_file=True)
    if not encoded_data:
        return None
    
    # Delta优化文件只有一个行（位置）
    combined_values = encoded_data[0]
    
    # 计算总长度（只考虑固定长度部分）
    total_fixed_length = sum(s for s in length_specs if s > 0)
    
    # 解码delta序列
    print(f"  解码Delta序列: {len(combined_values)} 个值...")
    decoded_values = decode_delta_optimized_sequence(combined_values, total_fixed_length)
    
    # 处理每个实例的拆分
    instance_data = []
    for value in decoded_values:
        # 将整数转换为字符串
        value_str = str(value)
        
        # 检查字符串长度是否不足
        if len(value_str) < total_fixed_length:
            # 前面补零
            value_str = value_str.zfill(total_fixed_length)
        elif len(value_str) > total_fixed_length:
            # 截断左侧多余的位数
            value_str = value_str[-total_fixed_length:]
        
        # 拆分成各个部分
        parts = []
        start = 0
        for length_spec in length_specs:
            if length_spec <= 0:  # 变长部分占位符
                parts.append("")
                continue
                
            end = start + length_spec
            part = value_str[start:end]
            parts.append(part)
            start = end
        
        instance_data.append(parts)
    
    print(f"  处理Delta优化文件: {os.path.basename(file_path)}, 实例数: {num_instances}")
    return instance_data

# Delta优化序列解码 TODO 获得delta后还需要拆分
def decode_delta_optimized_sequence(encoded_values, total_fixed_length):
    """
    解码Delta优化序列
    :param encoded_values: 编码值列表
    :param total_fixed_length: 固定部分总长度
    :return: 解码后的原始数值列表
    """
    if not encoded_values:
        return []
    
    # 第一个值就是基准值，不需要解码
    decoded_numbers = [encoded_values[0]]
    print(f"base_value: {decoded_numbers[0]}")
    
    # 处理后续的Delta值
    for i in range(1, len(encoded_values)):
        encoded = encoded_values[i]
        # 分离原始delta值和符号
        if encoded & 1 == 0:
            # 非负数：右移一位得到原始差值
            delta = encoded >> 1
        else:
            # 负数：右移一位后取负得到原始差值
            delta = -(encoded >> 1)
            # shifted = (encoded - 1) >> 1
            # delta = (1 << 63) - shifted;
            # print(f"encoded: {shifted}")
            print(f"delta: {delta}")
        
        # 计算当前实际值：前一个值 + delta
        current_value = decoded_numbers[-1] + delta
        decoded_numbers.append(current_value)
        print(f"current_value: {current_value}")
    
    # 验证所有值在有效范围内
    # max_value = 10 ** total_fixed_length - 1
    # min_value = 0
    
    # for i, value in enumerate(decoded_numbers):
    #     if value < min_value:
    #         print(f"警告: 解码值 {value} 小于最小值 ({min_value})")
    #         decoded_numbers[i] = min_value
    #     elif value > max_value:
    #         print(f"警告: 解码值 {value} 大于最大值 ({max_value})")
    #         decoded_numbers[i] = max_value
    
    return decoded_numbers

# 处理传统格式文件
def process_traditional_file(file_path, subtoken_dict):
    """
    处理传统格式的bin文件
    :param file_path: 文件路径
    :param subtoken_dict: 子标记字典
    :return: 实例数据列表
    """
    num_positions, num_instances, data = read_bin_file(file_path)
    if not data:
        return None
    
    # 转置矩阵：行->位置，列->实例
    instance_values = []
    for instance_idx in range(num_instances):
        values = []
        for position_idx in range(num_positions):
            values.append(data[position_idx][instance_idx])
        instance_values.append(values)
    
    # 转换为字符串
    converted_data = []
    for values in instance_values:
        converted = []
        for v in values:
            # 应用奇偶判断逻辑
            if v % 2 == 0:  # 数值
                num_val = v // 2
                value_str = str(num_val)
            else:  # 子标记ID
                token_id = (v - 1) // 2
                if token_id in subtoken_dict:
                    value_str = subtoken_dict[token_id]
                else:
                    value_str = f"<unknown_token:{token_id}>"
            converted.append(value_str)
        converted_data.append(converted)
    
    print(f"  处理传统文件: {os.path.basename(file_path)}, "
          f"位置数: {num_positions}, 实例数: {num_instances}")
    return converted_data

# 加载子标记字典
def load_subtoken_dictionary(dict_dir):
    """从目录中加载子标记字典"""
    dict_files = glob.glob(os.path.join(dict_dir, '*_subtoken_dictionary.txt'))
    if not dict_files:
        print(f"错误: 在目录 {dict_dir} 中未找到子标记字典文件")
        return None
    
    # 加载第一个找到的字典文件
    dict_file = dict_files[0]
    subtoken_dict = {}
    
    try:
        with open(dict_file, 'r', encoding='utf-8') as f:
            for idx, line in enumerate(f):
                subtoken_dict[idx] = line.strip()
        print(f"从 {os.path.basename(dict_file)} 加载了 {len(subtoken_dict)} 个子标记")
        return subtoken_dict
    except Exception as e:
        print(f"加载子标记字典失败: {str(e)}")
        return None

# 解析bin文件名
def parse_bin_filename(filename):
    """解析bin文件名，提取ID和长度信息"""
    basename = os.path.basename(filename)
    
    # 提取文件名中的数字部分
    parts = re.findall(r'[0-9-]+', basename)
    if not parts:
        return None, []
    
    try:
        # 第一个部分是ID
        id_num = int(parts[0])
        
        # 后续部分是长度规格
        length_specs = []
        for p in parts[1:]:
            try:
                # 所有长度规格都作为整数读取
                length_specs.append(int(p))
            except ValueError:
                # 无法解析为整数的部分设为-1（变长）
                length_specs.append(-1)
        
        return id_num, length_specs
    except ValueError:
        return None, []

# 替换复合子标记
def replace_composite_subtoken(id_num, subtoken_data):
    """
    替换复合子标记为实际值
    :param id_num: 子标记ID
    :param subtoken_data: 子标记数据存储
    :return: 替换后的字符串
    """
    if id_num not in subtoken_data:
        return f"<invalid_composite:{id_num}>"
    
    data = subtoken_data[id_num]
    pattern = data['pattern']
    placeholder_data = data['placeholder_data']
    counter = data['counter']
    
    # 检查是否有更多实例
    if counter >= len(placeholder_data):
        return f"<no_more_instances:{id_num}>"
    
    # 获取当前实例值
    current_values = placeholder_data[counter]
    
    # 更新计数器
    data['counter'] = counter + 1
    
    # 替换模式中的占位符
    parts = pattern.split('<>')
    result = parts[0]
    for i, val in enumerate(current_values):
        if i < len(parts) - 1:
            result += val + parts[i+1]
        else:
            result += val
    
    return result

# 替换简单子标记
def replace_simple_subtoken(id_num, subtoken_dict):
    """
    替换简单子标记
    :param id_num: 子标记ID
    :param subtoken_dict: 子标记字典
    :return: 替换后的字符串
    """
    if id_num in subtoken_dict:
        return subtoken_dict[id_num]
    return f"<unknown_token:{id_num}>"

# 主处理函数
def main():
    parser = argparse.ArgumentParser(description='处理日志文件并进行二进制数据替换')
    parser.add_argument('log_file', help='输入日志文件路径')
    parser.add_argument('data_dir', help='包含bin文件和字典的目录')
    parser.add_argument('output_file', help='输出文件路径')
    args = parser.parse_args()

    print(f"开始处理日志: {args.log_file}")
    
    # 加载子标记字典
    subtoken_dict = load_subtoken_dictionary(args.data_dir)
    if not subtoken_dict:
        print("错误: 无法加载子标记字典，退出")
        return
    
    # 识别复合子标记（包含<>的）
    composite_subtokens = {}
    for id_num, pattern in subtoken_dict.items():
        if '<>' in pattern:
            placeholder_count = pattern.count('<>')
            composite_subtokens[id_num] = placeholder_count
            print(f"  复合子标记 #{id_num}: '{pattern}' ({placeholder_count} 个占位符)")
    
    # 处理所有bin文件
    bin_files = glob.glob(os.path.join(args.data_dir, '*.bin'))
    subtoken_data = {}
    processed_bin_files = set()
    
    print(f"找到 {len(bin_files)} 个bin文件")
    for bin_file in bin_files:
        id_num, length_specs = parse_bin_filename(bin_file)
        if id_num is not None and id_num in composite_subtokens:
            is_delta_file = "delta" in os.path.basename(bin_file).lower()
            
            if is_delta_file:
                placeholder_data = process_delta_file(bin_file, length_specs)
            else:
                placeholder_data = process_traditional_file(bin_file, subtoken_dict)
            
            if not placeholder_data:
                print(f"警告: 无法处理文件 {os.path.basename(bin_file)}")
                continue
            
            # 存储结果
            subtoken_data[id_num] = {
                'pattern': subtoken_dict[id_num],
                'placeholder_data': placeholder_data,
                'counter': 0,
                'length_specs': length_specs,
                'is_delta_optimized': is_delta_file
            }
            
            processed_bin_files.add(os.path.basename(bin_file))
            
            # 打印加载信息
            specs_str = ", ".join(str(s) if s != 0 else "变长" for s in length_specs)
            print(f"  为ID {id_num} 加载了 {len(placeholder_data)} 个实例，长度规格: [{specs_str}]")
            print(f"  文件类型: {'Delta优化' if is_delta_file else '传统格式'}")
    
    # 处理日志文件
    processed_lines = []
    with open(args.log_file, 'r', encoding='utf-8') as f_in:
        for line_num, line in enumerate(f_in, 1):
            processed_line = line
            
            # 查找所有标记格式：|标记内容|
            markers = re.findall(r'\|([^\|]+)\|', line)
            for marker_str in markers:
                # 尝试解析为数字ID
                if marker_str.isdigit():
                    id_num = int(marker_str)
                # 否则尝试作为字母标签转换
                else:
                    id_num = letter_to_number(marker_str)
                
                # 检查子标记类型并替换
                if id_num in composite_subtokens:
                    replacement = replace_composite_subtoken(id_num, subtoken_data)
                else:
                    replacement = replace_simple_subtoken(id_num, subtoken_dict)
                
                # 替换标记
                processed_line = processed_line.replace(f"|{marker_str}|", replacement, 1)
            
            processed_lines.append(processed_line)
    
    # 写入输出文件
    with open(args.output_file, 'w', encoding='utf-8') as f_out:
        f_out.writelines(processed_lines)
    
    print(f"处理完成! 输出文件: {args.output_file}")
    
    # 打印未使用的bin文件
    unused_bin_files = set(os.path.basename(f) for f in bin_files) - processed_bin_files
    if unused_bin_files:
        print("\n警告: 以下bin文件未被使用:")
        for f in sorted(unused_bin_files):
            print(f"  - {f}")
    
    # 打印统计信息
    if subtoken_data:
        print("\n复合子标记使用统计:")
        for id_num, data in subtoken_data.items():
            counter = data['counter']
            total_instances = len(data['placeholder_data'])
            specs = ", ".join(str(s) if s != 0 else "变长" for s in data['length_specs'])
            file_type = "Delta优化" if data['is_delta_optimized'] else "传统格式"
            print(f"  ID {id_num} ({data['pattern']}): 使用了 {counter}/{total_instances} 个实例 ({file_type})")
            print(f"      长度规格: [{specs}]")

if __name__ == "__main__":
    main()