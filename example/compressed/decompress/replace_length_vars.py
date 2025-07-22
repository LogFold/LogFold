import sys
import os
import re
from collections import defaultdict

def get_key_from_length(length):
    """
    根据给定的长度返回对应的字母键值
    match length {
        1 => key = "a"
        2 => key = "b"
        3 => key = "c"
        4 => key = "d"
        5 => key = "e"
        6 => key = "f"
        7 => key = "g"
        8 => key = "h"
        9 => key = "i"
        10 => key = "j"
        11 => key = "k"
        12 => key = "l"
        13 => key = "m"
        14 => key = "n"
        15 => key = "o"
        _ => key = str(length)
    }
    """
    if 1 <= length <= 15:
        return chr(ord('a') + length - 1)
    return str(length)

def get_length_from_key(key):
    """
    根据给定的字母键值返回对应的长度
    """
    if len(key) == 1 and 'a' <= key <= 'o':
        return ord(key) - ord('a') + 1
    try:
        return int(key)
    except ValueError:
        return None

def parse_a_file(a_path):
    """
    解析a.txt文件，提取所有占位符及其位置
    返回：包含每个占位符位置和键值的列表
    """
    placeholder_pattern = r'<([a-z])>' #不包括0-9和*
    placeholders = []
    
    with open(a_path, 'r', encoding='utf-8') as f:
        content = f.read()
        
        # 查找所有占位符
        for match in re.finditer(placeholder_pattern, content):
            key = match.group(1)
            start, end = match.span()
            
            # 如果占位符中没有键值，表示为普通占位符
            if not key:
                # 使用索引作为隐式键值（从0开始）
                key = str(len(placeholders))
            
            placeholders.append({
                'start': start,
                'end': end,
                'key': key
            })
    
    return placeholders, content

def load_decompressed_files(b_directory):
    """
    加载b-directory中的所有解压文件
    返回：按文件长度分组的数值列表
    """
    file_content = defaultdict(list)
    
    # 遍历b-directory中的所有文件
    for filename in os.listdir(b_directory):
        # 匹配 lx_0_decompressed.txt 格式的文件名
        if filename.startswith('l') and filename.endswith('_0_decompressed.txt'):
            try:
                # 提取长度值
                length_str = filename.split('_')[0][1:]
                if not length_str:
                    continue
                    
                length = int(length_str)
                
                # 读取文件内容
                with open(os.path.join(b_directory, filename), 'r', encoding='utf-8') as f:
                    values = [line.strip() for line in f.readlines() if line.strip()]
                    file_content[length] = values
                    
                print(f"Loaded {len(values)} values from {filename} (length={length})")
            except (ValueError, IndexError) as e:
                print(f"Skipping invalid filename format: {filename} - {str(e)}")
    
    return file_content

def replace_placeholders(a_path, b_directory):
    """
    替换a.txt中的占位符并生成新文件
    """
    # 1. 解析a.txt文件
    placeholders, original_content = parse_a_file(a_path)
    print(f"Found {len(placeholders)} placeholders in {a_path}")
    
    # 2. 加载所有解压文件
    file_content = load_decompressed_files(b_directory)
    
    # 3. 为每个占位符准备数值
    key_counts = defaultdict(int)
    replaced_values = []
    missing_keys = set()
    
    for idx, placeholder in enumerate(placeholders):
        key = placeholder['key']
        length = get_length_from_key(key)
        
        # 如果无法解析键值对应的长度
        if length is None:
            print(f"Warning: Invalid key format - '<{key}>' at position {placeholder['start']}")
            length = idx + 1
            length_from_key = length
        else:
            length_from_key = length
        
        # 尝试获取该长度对应的文件内容
        if length_from_key not in file_content:
            missing_keys.add(str(length_from_key))
            print(f"Warning: No file for length {length_from_key} (key: {key})")
            replaced_value = f"<missing:{key}>"
        else:
            # 从该长度的文件中获取下一个值
            values = file_content[length_from_key]
            count = key_counts[length_from_key]
            
            if count < len(values):
                replaced_value = values[count]
                key_counts[length_from_key] += 1
            else:
                replaced_value = f"<empty:{key}>"
                print(f"Warning: Not enough values for length {length_from_key} (key: {key}), needs index {count}")
        
        replaced_values.append(replaced_value)
    
    # 4. 替换占位符
    # 由于我们要替换多个位置，需要从后往前替换以避免索引变化
    content_list = list(original_content)
    
    for i in range(len(placeholders)-1, -1, -1):
        placeholder = placeholders[i]
        start = placeholder['start']
        end = placeholder['end']
        content_list[start:end] = replaced_values[i]
    
    replaced_content = ''.join(content_list)
    
    # 5. 保存结果
    output_path = os.path.splitext(a_path)[0] + '_replaced.txt'
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(replaced_content)
    
    # 6. 打印统计信息
    print("\nReplacement summary:")
    print(f"  Total placeholders processed: {len(placeholders)}")
    print(f"  Values used: {sum(key_counts.values())}")
    
    if missing_keys:
        print(f"  Missing keys: {', '.join(sorted(missing_keys))}")
    
    for length, count in key_counts.items():
        print(f"  Length {length}: used {count} values")
    
    print(f"\nReplaced file saved to: {output_path}")
    return replaced_content, output_path

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python replace_incomplete_pattern.py a.txt b-directory")
        print("Replaces placeholders in a.txt with values from decompressed files in b-directory")
        print("Placeholder format: <key>")
        print("Example: |42| |7| - INFO  |31| - binding to port <*> set to <a>")
        print("Output: <original_filename>_replaced.txt")
        sys.exit(1)
    
    a_path = sys.argv[1]
    b_directory = sys.argv[2]
    
    if not os.path.isfile(a_path):
        print(f"Error: a.txt not found at {a_path}")
        sys.exit(1)
    
    if not os.path.isdir(b_directory):
        print(f"Error: b-directory not found at {b_directory}")
        sys.exit(1)
    
    print(f"Processing a.txt: {a_path}")
    print(f"Using b-directory: {b_directory}")
    
    try:
        replaced_content, output_path = replace_placeholders(a_path, b_directory)
        print("Replacement completed successfully!")
    except Exception as e:
        print(f"Error: {str(e)}")
        import traceback
        traceback.print_exc()
        sys.exit(1)