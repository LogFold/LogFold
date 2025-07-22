import os
import re
import glob
import leb128
import argparse
from collections import defaultdict
import math

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

def decode_delta_sequence(deltas):
    """解码delta序列为原始值序列"""
    if not deltas:
        return []
    
    values = [deltas[0]]
    for i in range(1, len(deltas)):
        values.append(values[-1] + deltas[i])
    
    return values

def format_value(value_str, length_spec):
    """
    根据长度规格格式化值
    :param value_str: 值的字符串表示
    :param length_spec: 长度规格 (整数值表示固定长度，-1表示不固定)
    :return: 格式化后的值
    """
    if length_spec == -1:  # 变长，直接返回
        return value_str
    
    # 如果是固定长度
    target_length = int(length_spec)
    
    # 如果实际长度小于目标长度，在前面补0
    if len(value_str) < target_length:
        return value_str.zfill(target_length)
    
    # 如果实际长度大于目标长度，发出警告
    if len(value_str) > target_length:
        print(f"警告: 值 '{value_str}' 超过目标长度 {target_length}，进行截断处理")
    
    # 返回长度不超过目标长度的值
    return value_str[:target_length]

def process_bin_file(bin_path, placeholder_count, length_specs):
    """
    处理单个.bin文件并返回解码后的数据
    :param bin_path: 二进制文件路径
    :param placeholder_count: 每个实例的占位符个数
    :param length_specs: 每个占位符的长度规格
    :return: 实例列表 [每个实例是一个字符串列表]
    """
    with open(bin_path, 'rb') as f:
        try:
            # 读取行数和列数 (行数 = 占位符位置数, 列数 = 实例数)
            num_placeholder_positions = leb128.u.decode_reader(f)[0]  # 占位符位置的数量
            num_instances = leb128.u.decode_reader(f)[0]  # 实例数量
            
            # 验证矩阵维度是否匹配
            if placeholder_count != num_placeholder_positions:
                print(f"警告: 文件 {os.path.basename(bin_path)} 的占位符位置数 ({num_placeholder_positions}) "
                      f"与预期占位符数 ({placeholder_count}) 不匹配")
            
            # 验证长度规格与占位符位置数是否匹配
            if len(length_specs) != num_placeholder_positions:
                print(f"警告: 文件 {os.path.basename(bin_path)} 的长度规格数 ({len(length_specs)}) "
                      f"与占位符位置数 ({num_placeholder_positions}) 不匹配")
            
            # 读取每个占位符位置的数据
            matrix = []
            for _ in range(num_placeholder_positions):
                # 读取编码标记
                flag = leb128.u.decode_reader(f)[0]
                
                if flag == 0:  # 原始值
                    row_data = []
                    for _ in range(num_instances):
                        value = leb128.u.decode_reader(f)[0]
                        row_data.append(value)
                    matrix.append(row_data)
                elif flag == 1:  # Delta编码
                    deltas = []
                    for _ in range(num_instances):
                        d = leb128.i.decode_reader(f)[0]
                        deltas.append(d)
                    row_data = decode_delta_sequence(deltas)
                    matrix.append(row_data)
                else:
                    print(f"错误: 文件 {os.path.basename(bin_path)} 未知的编码标记: {flag}")
                    return None
            
            # 矩阵的行代表占位符位置，列代表实例
            # 重新组织数据: 实例 -> 占位符值
            instance_data = []
            for instance_idx in range(num_instances):
                instance_values = []
                for placeholder_idx in range(num_placeholder_positions):
                    instance_values.append(matrix[placeholder_idx][instance_idx])
                instance_data.append(instance_values)
            
            print(f"  处理文件: {os.path.basename(bin_path)}, "
                  f"占位符位置数: {num_placeholder_positions}, 实例数: {num_instances}")
            return instance_data
        except Exception as e:
            print(f"处理文件 {os.path.basename(bin_path)} 时出错: {str(e)}")
            return None

def load_subtoken_dictionary(dict_dir):
    """
    从目录中加载子标记字典
    :param dict_dir: 包含子标记字典文件的目录
    :return: 字典 {id: subtoken_string}
    """
    dict_files = glob.glob(os.path.join(dict_dir, '*_subtoken_dictionary.txt'))
    if not dict_files:
        print(f"错误: 在目录 {dict_dir} 中未找到子标记字典文件")
        return None
    
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

def convert_values(values, subtoken_dict, length_specs):
    """
    转换值列表为字符串列表，并根据长度规格格式化
    :param values: 值列表 (u64)
    :param subtoken_dict: 子标记字典
    :param length_specs: 每个值的长度规格
    :return: 字符串列表
    """
    result = []
    for idx, v in enumerate(values):
        if idx >= len(length_specs):
            # 如果没有对应的长度规格，使用默认值 -1（不固定）
            length_spec = -1
        else:
            length_spec = length_specs[idx]
        
        # 转换原始值
        if v % 2 == 0:  # 数值
            num_val = v // 2
            value_str = str(num_val)
        else:  # 子标记ID
            token_id = (v - 1) // 2
            if token_id in subtoken_dict:
                value_str = subtoken_dict[token_id]
            else:
                value_str = f"<unknown_token:{token_id}>"
        
        # 根据长度规格格式化值
        formatted_value = format_value(value_str, length_spec)
        result.append(formatted_value)
    
    return result

def parse_bin_filename(filename):
    """
    解析bin文件名，提取ID和长度信息
    :param filename: 文件名 (带或不带路径)
    :return: (id, 长度规格列表)
    """
    basename = os.path.basename(filename)
    # 提取文件名中的数字部分
    parts = re.findall(r'[0-9-]+', basename)
    if not parts:
        return None, []
    
    try:
        id_num = int(parts[0])
        length_specs = []
        for p in parts[1:]:
            try:
                length_specs.append(int(p))
            except ValueError:
                # 无法解析为整数的部分设为 -1（不固定长度）
                length_specs.append(-1)
        return id_num, length_specs
    except ValueError:
        return None, []

def get_marker_value(marker_str):
    """
    解析标记字符串，返回数字ID（支持字母和数字格式）
    """
    if not marker_str:
        return None
    
    # 尝试解析为数字
    if marker_str.isdigit():
        return int(marker_str)
    
    # 尝试解析为字母序列（转换数字ID）
    if marker_str.isalpha():
        return letter_to_number(marker_str)
    
    # 无法解析的类型
    print(f"警告: 无法解析标记 '{marker_str}'，既不是纯数字也不是纯字母")
    return None

def process_composite_subtoken(id_num, subtoken_dict, bin_data_dir, subtoken_data):
    """
    处理复合子标记，解压其对应的bin文件
    :param id_num: 子标记ID
    :param subtoken_dict: 子标记字典
    :param bin_data_dir: 包含bin文件的目录
    :param subtoken_data: 用于存储结果的字典
    """
    # 查找匹配的bin文件
    bin_files = glob.glob(os.path.join(bin_data_dir, f"_{id_num}_*.bin"))
    if not bin_files:
        print(f"警告: 未找到ID {id_num} 的bin文件")
        return None
    
    bin_file = bin_files[0]  # 取第一个匹配的文件
    pattern = subtoken_dict[id_num]
    placeholder_count = pattern.count("<>")
    
    # 从文件名解析长度规格
    file_id, length_specs = parse_bin_filename(bin_file)
    
    # 验证文件ID与标记ID是否一致
    if file_id is None or file_id != id_num:
        print(f"警告: bin文件ID不匹配 - 文件名: {os.path.basename(bin_file)}, 期望ID: {id_num}")
        if file_id is not None:
            # 尽量使用文件名中的ID
            id_num = file_id
        else:
            return None
    
    # 处理bin文件 (矩阵行数 = 占位符位置数)
    instance_data = process_bin_file(bin_file, placeholder_count, length_specs)
    if not instance_data:
        print(f"错误: 无法处理ID {id_num} 的bin文件")
        return None
    
    # 将每个实例的值转换为字符串（应用长度规格）
    placeholder_data = []
    for instance_values in instance_data:
        converted_values = convert_values(instance_values, subtoken_dict, length_specs)
        placeholder_data.append(converted_values)
    
    # 存储结果
    subtoken_data[id_num] = {
        'pattern': pattern,
        'placeholder_data': placeholder_data,  # 列表的列表: [实例][占位符位置]
        'counter': 0,  # 实例计数器
        'length_specs': length_specs  # 存储长度规格
    }
    
    # 打印长度规格信息
    specs_str = ", ".join(str(s) if s != -1 else "变长" for s in length_specs)
    print(f"  为ID {id_num} 加载了 {len(placeholder_data)} 个实例，长度规格: [{specs_str}]")
    return bin_file

def replace_composite_subtoken(id_num, subtoken_data):
    """
    替换复合子标记
    :param id_num: 子标记ID
    :param subtoken_data: 复合子标记数据
    :return: 替换后的字符串
    """
    if id_num not in subtoken_data:
        return f"<invalid_composite:{id_num}>"
    
    data = subtoken_data[id_num]
    pattern = data['pattern']
    placeholder_data = data['placeholder_data']
    counter = data['counter']
    
    # 检查是否有更多实例可用
    if counter >= len(placeholder_data):
        return f"<no_more_instances:{id_num}>"
    
    # 获取当前实例的占位符值
    current_values = placeholder_data[counter]
    
    # 更新计数器
    data['counter'] = counter + 1
    
    # 使用当前值替换占位符
    parts = pattern.split('<>')
    replaced_pattern = parts[0]
    
    for i, val in enumerate(current_values):
        if i < len(parts) - 1:
            replaced_pattern += val + parts[i+1]
        else:
            replaced_pattern += val
    
    return replaced_pattern

def replace_simple_subtoken(id_num, subtoken_dict):
    """
    替换简单子标记（不包含占位符）
    :param id_num: 子标记ID
    :param subtoken_dict: 子标记字典
    :return: 替换后的字符串
    """
    if id_num in subtoken_dict:
        return subtoken_dict[id_num]
    return f"<unknown_token:{id_num}>"

def main():
    parser = argparse.ArgumentParser(description='处理日志文件并进行二进制数据替换')
    parser.add_argument('log_file', help='输入日志文件')
    parser.add_argument('data_dir', help='包含bin文件、字典和日志文件的目录')
    parser.add_argument('output_file', help='输出文件路径')
    args = parser.parse_args()

    print(f"开始处理日志: {args.log_file}")
    
    # 1. 加载子标记字典
    subtoken_dict = load_subtoken_dictionary(args.data_dir)
    if subtoken_dict is None:
        print("无法继续处理，退出")
        return
    
    # 2. 识别复合子标记（包含<>的）
    composite_subtokens = {}
    for id_num, pattern in subtoken_dict.items():
        if '<>' in pattern:
            placeholder_count = pattern.count('<>')
            composite_subtokens[id_num] = placeholder_count
            print(f"  复合子标记 #{id_num}: '{pattern}' ({placeholder_count} 个占位符)")
    
    # 3. 处理所有bin文件
    bin_files = glob.glob(os.path.join(args.data_dir, '*.bin'))
    subtoken_data = {}  # 存储复合子标记的数据
    processed_bin_files = set()
    
    print(f"找到 {len(bin_files)} 个bin文件")
    for bin_file in bin_files:
        id_num, _ = parse_bin_filename(bin_file)
        if id_num is not None and id_num in composite_subtokens:
            # 处理复合子标记的bin文件
            processed_file = process_composite_subtoken(id_num, subtoken_dict, args.data_dir, subtoken_data)
            if processed_file:
                processed_bin_files.add(os.path.basename(processed_file))
    
    # 4. 处理日志文件
    processed_lines = []
    with open(args.log_file, 'r', encoding='utf-8') as f_in:
        for line_num, line in enumerate(f_in, 1):
            processed_line = line
            
            # 查找所有标记格式：|标记内容|
            markers = re.findall(r'\|([^\|]+)\|', line)
            for marker_str in markers:
                id_num = get_marker_value(marker_str)
                if id_num is None:
                    replacement = f"<invalid_marker:{marker_str}>"
                    processed_line = processed_line.replace(f"|{marker_str}|", replacement, 1)
                    continue
                
                # 检查是否是复合标记
                if id_num in composite_subtokens:
                    replacement = replace_composite_subtoken(id_num, subtoken_data)
                else:
                    replacement = replace_simple_subtoken(id_num, subtoken_dict)
                
                # 替换标记
                processed_line = processed_line.replace(f"|{marker_str}|", replacement, 1)
            
            processed_lines.append(processed_line)
    
    # 5. 写入输出文件
    with open(args.output_file, 'w', encoding='utf-8') as f_out:
        f_out.writelines(processed_lines)
    
    print(f"处理完成! 输出文件: {args.output_file}")
    
    # 打印未处理的bin文件
    unused_bin_files = set(os.path.basename(f) for f in bin_files) - processed_bin_files
    if unused_bin_files:
        print("\n警告: 以下bin文件未被使用:")
        for f in sorted(unused_bin_files):
            print(f"  - {f}")
    
    # 打印复合子标记使用统计
    if subtoken_data:
        print("\n复合子标记使用统计:")
        for id_num, data in subtoken_data.items():
            counter = data['counter']
            total_instances = len(data['placeholder_data'])
            specs = ", ".join(str(s) if s != -1 else "变长" for s in data['length_specs'])
            print(f"  ID {id_num} ({data['pattern']}): 使用了 {counter}/{total_instances} 个实例, 长度规格: [{specs}]")

if __name__ == "__main__":
    main()