import leb128
import sys
import os
import re

def decompress_binary_file(input_path, output_path=None, fixed_length=None):
    """
    解压使用特定格式压缩的二进制文件
    
    压缩格式:
    1. 1字节标志: 
       - 0: 原始数据
       - 1: delta压缩数据
    2. 剩余部分是用LEB128编码的数值
    
    对于delta压缩:
      - 第一个值是原始值
      - 后续每个值都是与前一个值的差值
    """
    if not os.path.isfile(input_path):
        raise FileNotFoundError(f"输入文件不存在: {input_path}")
    
    if output_path is None:
        output_path = os.path.splitext(input_path)[0] + "_decompressed.txt"
    
    try:
        with open(input_path, "rb") as f_in, open(output_path, "w") as f_out:
            # 读取压缩模式标志
            compression_flag = leb128.u.decode_reader(f_in)[0]
            
            print(f"压缩模式: {'Delta压缩' if compression_flag == 1 else '原始数据'}")
            if fixed_length is not None and fixed_length > 0:
                print(f"固定长度: {fixed_length} (小于此长度的值将补前导零)")
            
            # 读取第一个值 (无论什么模式都同样处理)
            first_value = leb128.i.decode_reader(f_in)[0]
            
            # 将第一个值写入文本文件
            formatted_value = format_value(first_value, fixed_length)
            f_out.write(f"{formatted_value}\n")
            
            # 处理后续值
            if compression_flag == 0:
                # 原始数据模式: 所有值直接写入文本文件
                print("处理原始数据模式")
                
                # 继续读取所有剩余值并写入文本文件
                while f_in.peek():
                    try:
                        value = leb128.i.decode_reader(f_in)[0]
                        formatted_value = format_value(value, fixed_length)
                        f_out.write(f"{formatted_value}\n")
                    except Exception as e:
                        print(f"解码错误: {str(e)}")
                        break
                
            elif compression_flag == 1:
                # Delta压缩模式: 需要重建原始值
                print("处理Delta压缩模式")
                current_value = first_value
                
                # 读取所有delta值并重建原始值
                while f_in.peek():
                    try:
                        delta = leb128.i.decode_reader(f_in)[0]
                        current_value += delta
                        formatted_value = format_value(current_value, fixed_length)
                        f_out.write(f"{formatted_value}\n")
                    except Exception as e:
                        print(f"解码错误: {str(e)}")
                        break
                
            else:
                raise ValueError(f"无效的压缩标志: {compression_flag}")
            
        # 统计结果
        with open(output_path, 'r') as f_check:
            line_count = sum(1 for _ in f_check)
        
        print(f"成功解压文件到: {output_path}")
        print(f"解压行数: {line_count}")
        return output_path
        
    except Exception as e:
        print(f"解压过程中出错: {str(e)}")
        if os.path.exists(output_path):
            os.remove(output_path)
        raise

def format_value(value, fixed_length=None):
    """
    根据固定长度格式化数值
    :param value: 整数值
    :param fixed_length: 指定的固定长度 (None或-1表示不变)
    :return: 格式化后的字符串
    """
    # 转换为字符串
    value_str = str(value)
    
    # 处理固定长度
    if fixed_length is not None and fixed_length > 0:
        value_len = len(value_str)
        
        if value_len < fixed_length:
            # 小于指定长度 - 前导零补齐
            return value_str.zfill(fixed_length)
        elif value_len > fixed_length:
            # 大于指定长度 - 发出警告并截断
            print(f"警告: 值 '{value_str}' 超过指定长度 {fixed_length}，进行截断处理")
            return value_str[:fixed_length]
        else:
            # 正好指定长度
            return value_str
    
    # 未指定固定长度，返回原始字符串
    return value_str

def parse_length_from_filename(filename):
    """
    从文件名解析长度值 (lx_0.bin 中的 x)
    :param filename: 文件名
    :return: 长度值 (int)，如果无法解析返回 None
    """
    # 匹配 l开头，数字，然后是_0.bin的文件名
    match = re.search(r'^l(\d+)_[^\.]+\.bin$', filename)
    if match:
        try:
            return int(match.group(1))
        except ValueError:
            pass
    return None

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("使用说明: python decompress_to_txt.py <input_directory>")
        print("功能: 解压目录下的所有特定格式压缩文件为文本文件")
        print("解压格式:")
        print("  1. 输出文件为纯文本格式，每行一个整数值")
        print("  2. 文件扩展名: 解压后为.txt")
        print("  3. 文件名匹配: 处理所有以 'l' 开头, 以 '.bin' 结尾的文件")
        sys.exit(1)
    
    input_directory = sys.argv[1]
    
    if not os.path.isdir(input_directory):
        print(f"错误: 目录不存在 - {input_directory}")
        sys.exit(1)
    
    print(f"开始处理目录: {input_directory}")
    
    processed_files = 0
    skipped_files = 0
    
    for file in sorted(os.listdir(input_directory)):
        if file.startswith("l") and file.endswith(".bin"):
            input_file = os.path.join(input_directory, file)
            print("\n" + "-" * 50)
            print(f"处理文件: {file}")
            print(f"文件大小: {os.path.getsize(input_file):,} 字节")
            
            # 从文件名解析长度值
            fixed_length = parse_length_from_filename(file)
            if fixed_length is not None:
                print(f"检测到固定长度: {fixed_length}")
            
            try:
                # 调用解压函数，传入长度信息
                decompress_binary_file(input_file, fixed_length=fixed_length)
                processed_files += 1
            except Exception as e:
                print(f"文件处理失败: {str(e)}")
                skipped_files += 1
    
    print("\n" + "=" * 50)
    print(f"处理完成! 成功解压: {processed_files} 个文件, 跳过: {skipped_files} 个文件")
    if skipped_files == 0:
        print("所有文件都成功处理!")
    else:
        print(f"警告: 有 {skipped_files} 个文件处理失败")