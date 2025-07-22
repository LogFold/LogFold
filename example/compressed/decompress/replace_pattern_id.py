import re
import sys

def main():
    if len(sys.argv) != 3:
        print("使用说明: python replace_incomplete_pattern.py a.txt b.txt")
        print("请将两个文件拖放到脚本上或作为参数提供")
        print("a.txt: 包含占位符ID的日志文件")
        print("b.txt: ID到字符串的映射文件")
        sys.exit(1)
    
    log_file = sys.argv[1]
    mapping_file = sys.argv[2]
    
    try:
        # 第一步：构建ID到字符串的映射（从b.txt）
        id_to_string = {}
        
        with open(mapping_file, 'r', encoding='utf-8') as f:
            for id_val, line in enumerate(f):
                # 移除行尾换行符
                content = line.rstrip('\n')
                # 存储到映射表 (ID为行号，从0开始)
                id_to_string[id_val] = content
        
        print(f"从 {mapping_file} 加载了 {len(id_to_string)} 个映射项")
        
        # 第二步：处理a.txt文件
        output_file = log_file.replace('.txt', '_replaced.txt')
        
        with open(log_file, 'r', encoding='utf-8') as in_f, \
             open(output_file, 'w', encoding='utf-8') as out_f:
            
            print(f"处理日志文件: {log_file}")
            print(f"输出文件: {output_file}")
            
            # 定义正则表达式模式：匹配 |数字| 格式
            pattern = r'\|(\d+)\|'
            
            # 统计信息
            total_replacements = 0
            missing_mappings = set()
            
            # 逐行处理日志文件
            for line_num, line in enumerate(in_f, 1):
                # 替换占位符ID
                def replace_match(match):
                    nonlocal total_replacements
                    # 提取匹配的ID
                    id_str = match.group(1)
                    try:
                        id_val = int(id_str)
                        # 查找映射值
                        if id_val in id_to_string:
                            total_replacements += 1
                            return id_to_string[id_val]
                        else:
                            # 记录缺失的映射
                            missing_mappings.add(id_val)
                            return f"|{id_val}|"  # 保留原始标记
                    except ValueError:
                        # ID不是数字，保留原始内容
                        return match.group(0)
                
                # 使用正则表达式替换所有匹配项
                processed_line = re.sub(pattern, replace_match, line)
                out_f.write(processed_line)
        
        # 第三步：输出统计信息
        print(f"完成! 共替换了 {total_replacements} 个占位符ID")
        
        if missing_mappings:
            print(f"警告: 缺失映射的ID数量: {len(missing_mappings)}")
            # 按顺序打印缺失ID
            sorted_missing = sorted(missing_mappings)
            print("缺失的ID列表: " + ", ".join(map(str, sorted_missing)))
        
    except FileNotFoundError as e:
        print(f"错误: 文件未找到 - {e.filename}")
    except Exception as e:
        print(f"处理文件时出错: {str(e)}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()