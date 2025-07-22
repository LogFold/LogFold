import sys

def read_uleb128(data):
    """
    从字节数据中读取无符号 LEB128 编码的整数
    返回 (解析出的整数, 消耗的字节数)
    """
    value = 0
    shift = 0
    bytes_consumed = 0
    
    for byte in data:
        # 转换为无符号整数
        byte_val = byte
        # 取低7位
        value |= (byte_val & 0x7F) << shift
        shift += 7
        bytes_consumed += 1
        
        # 检查最高位是否为0（表示结束）
        if (byte_val & 0x80) == 0:
            break
    
    return value, bytes_consumed

def decompress_buffer(data):
    """
    解压缩符合 Rust LEB128 编码的数据
    返回解压出的整数列表
    """
    values = []
    index = 0
    total_length = len(data)
    
    while index < total_length:
        value, consumed = read_uleb128(data[index:])
        values.append(value)
        index += consumed
    
    return values

def main():
    if len(sys.argv) != 2:
        print("使用说明: python decompress.py input_file.bin")
        print("请将压缩文件拖放到脚本上或作为参数提供")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    try:
        # 读取二进制文件
        with open(input_file, "rb") as f:
            compressed_data = f.read()
        
        # 解压缩数据
        decompressed = decompress_buffer(compressed_data)
        
        # 打印结果
        print("解压后的整数列表:")
        for i, num in enumerate(decompressed):
            print(f"[{i}] = {num}")
        
        #可选: 输出到文件
        with open("decompressed-d-ids.txt", "w") as f:
            for num in decompressed:
                f.write(f"{num}\n")
        
    except FileNotFoundError:
        print(f"错误: 文件 '{input_file}' 未找到")
    except Exception as e:
        print(f"处理文件时出错: {str(e)}")

if __name__ == "__main__":
    main()