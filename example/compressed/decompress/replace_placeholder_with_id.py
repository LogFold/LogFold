import sys

def main():
    # 检查命令行参数
    if len(sys.argv) != 3:
        print("使用说明: python restore_log.py a.txt b.txt")
        print("请将两个文件拖放到脚本上或作为参数提供")
        print("a.txt: 原始日志文件")
        print("b.txt: decompress.py输出的数字列表文件")
        sys.exit(1)
    
    a_file = sys.argv[1]
    b_file = sys.argv[2]
    
    try:
        # 1. 从b.txt加载解压后的数字列表
        num_list = []
        with open(b_file, 'r') as f:
            for line in f:
                # 提取数字值（跳过注释行）
                # if "=" in line:
                #     parts = line.split("=")
                #     if len(parts) >= 2:
                #         try:
                #             num = int(parts[1].strip())
                #             num_list.append(num)
                #         except ValueError:
                #             continue
                num = int(line.strip())
                num_list.append(num)
        
        # 检查是否成功获取数字列表
        if not num_list:
            print(f"错误: 在 {b_file} 中没有找到有效数字")
            sys.exit(1)
        
        print(f"从 {b_file} 加载了 {len(num_list)} 个替换数字")
        
        # 2. 处理a.txt文件
        index = 0  # 当前使用的数字索引
        total_replacements = 0  # 总替换次数统计
        
        with open(a_file, 'r') as in_file:
            # 输出文件名
            output_file = a_file.replace('.txt', '_restored.txt')
            with open(output_file, 'w') as out_file:
                print(f"处理日志文件: {a_file}")
                print(f"输出文件: {output_file}")
                
                # 逐行处理
                for line in in_file:
                    # 统计当前行中的 <*> 数量
                    placeholders = line.count('<*>')
                    
                    # 如果没有占位符，直接写入
                    if placeholders == 0:
                        out_file.write(line)
                        continue
                    
                    # 准备替换
                    new_line = line
                    
                    # 对每个占位符进行替换
                    for _ in range(placeholders):
                        if index < len(num_list):
                            # 只替换第一个出现的占位符
                            new_line = new_line.replace('<*>', f"|{str(num_list[index])}|", 1)
                            index += 1
                            total_replacements += 1
                        else:
                            # 数字不够用，保留剩余的<*>
                            break
                    
                    # 写入处理后的行
                    out_file.write(new_line)
        
        print(f"完成! 共替换了 {total_replacements} 个占位符")
        
        # 检查是否有未使用的数字
        if index < len(num_list):
            print(f"警告: 有 {len(num_list) - index} 个未使用的数字")
        
    except FileNotFoundError as e:
        print(f"错误: 文件未找到 - {e.filename}")
    except Exception as e:
        print(f"处理文件时出错: {str(e)}")

if __name__ == "__main__":
    main()