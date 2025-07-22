import sys

def main():
    if len(sys.argv) != 3:
        print("使用说明: python restore_ordered_log.py a.txt b.txt")
        print("请将两个文件拖放到脚本上或作为参数提供")
        print("a.txt: 原始日志文件")
        print("b.txt: 数字ID序列文件")
        sys.exit(1)
    
    a_file = sys.argv[1]
    b_file = sys.argv[2]
    
    try:
        # 第一步：从a.txt读取日志并构建行内容到ID的映射
        content_to_id = {}
        id_to_content = []
        unique_id = 0
        
        with open(a_file, 'r', encoding='utf-8') as f:
            for line in f:
                # 移除行尾换行符以进行内容比较
                stripped_line = line.rstrip('\n')
                
                # 检查是否已存在相同内容
                if stripped_line in content_to_id:
                    # 已有相同内容，使用现有ID
                    id_val = content_to_id[stripped_line]
                else:
                    # 新内容，分配新ID
                    content_to_id[stripped_line] = unique_id
                    id_val = unique_id
                    id_to_content.append(stripped_line)
                    unique_id += 1
        
        print(f"在 {a_file} 中找到 {len(content_to_id)} 个唯一日志条目")
        
        # 第二步：读取b.txt中的ID序列
        id_sequence = []
        with open(b_file, 'r', encoding='utf-8') as f:
            for line_num, line in enumerate(f, 1):
                stripped = line.strip()
                if not stripped:
                    continue
                
                try:
                    num = int(stripped)
                    id_sequence.append(num)
                except ValueError:
                    print(f"警告: 在{b_file}第{line_num}行发现无效数字: {stripped}")
        
        print(f"从 {b_file} 加载了 {len(id_sequence)} 个ID")
        
        # 第三步：根据ID序列构建还原日志
        output_file = a_file.replace('.txt', '_new.txt')
        
        with open(output_file, 'w', encoding='utf-8') as out_f:
            # 统计使用的ID情况
            used_ids = set()
            missing_ids = set()
            
            for id_val in id_sequence:
                if 0 <= id_val < len(id_to_content):
                    # 获取日志内容并添加换行符
                    content = id_to_content[id_val] + '\n'
                    out_f.write(content)
                    used_ids.add(id_val)
                else:
                    # 记录缺失的ID
                    print(f"警告: 无效ID {id_val} (最大有效ID为 {len(id_to_content)-1})")
                    out_f.write(f"<INVALID_ID:{id_val}>\n")
                    missing_ids.add(id_val)
        
        # 第四步：生成统计信息
        print(f"已创建还原文件: {output_file}")
        print(f"使用的唯一ID数量: {len(used_ids)}")
        
        if missing_ids:
            print(f"警告: 发现 {len(missing_ids)} 个无效ID")
        
        # 检查未使用的ID
        unused_ids = set(range(len(id_to_content))) - used_ids
        if unused_ids:
            print(f"警告: 有 {len(unused_ids)} 个日志条目未使用")
        
    except FileNotFoundError as e:
        print(f"错误: 文件未找到 - {e.filename}")
    except Exception as e:
        print(f"处理文件时出错: {str(e)}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()