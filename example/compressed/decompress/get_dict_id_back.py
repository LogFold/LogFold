import re
import sys

def letter_to_number(s: str) -> int:
    """
    将小写字母序列转换回原始数字
    转换规则与Rust函数number_to_letter对应
    """
    if len(s) == 0:
        return 0
        
    if len(s) == 1:
        # 单个字母: a->0, b->1, ... z->25
        return ord(s) - ord('a')
    
    # 多个字母: aa->26, ab->27, ... aaa->702, 等等
    total = 0
    # 对于n位字母序列，总数为 (26^n - 26)/25 + ∑(字母值 * 26^(位置权重))
    for i, char in enumerate(s):
        char_val = ord(char) - ord('a')
        # 每个位置的权重
        total += char_val * (26 ** (len(s) - i - 1))
    
    # 添加基值部分 (等比数列求和)
    base = (26**len(s) - 26) // 25
    return base + total

def convert_log_line(line: str) -> str:
    """
    处理日志行，将 |字母序列| 格式转换为数字
    """
    # 使用正则表达式找到所有 |字母序列| 格式的部分
    pattern = r'\|([a-z]+)\|'
    
    def replace_match(match):
        letters = match.group(1)
        try:
            number = letter_to_number(letters)
            return f"|{number}|"
        except:
            # 转换失败时保留原始内容
            return match.group(0)
    
    # 替换所有匹配的部分
    return re.sub(pattern, replace_match, line)

def main():
    if len(sys.argv) != 2:
        print("使用方法: python convert_log.py 日志文件.txt")
        print("请将日志文件拖放到脚本上或作为参数提供")
        sys.exit(1)
    
    input_file = sys.argv[1]
    
    try:
        with open(input_file, 'r', encoding='utf-8') as f:
            for line in f:
                processed_line = convert_log_line(line)
                # 保留行尾的换行符
                print(processed_line, end='')
    except FileNotFoundError:
        print(f"错误: 文件 '{input_file}' 未找到")
    except Exception as e:
        print(f"处理文件时出错: {str(e)}")

if __name__ == "__main__":
    main()