import random
import sys
import os

def compare_random_lines(file_a, file_b, sample_size=10):
    """
    随机抽样比较两个文件的行内容
    
    参数:
        file_a (str): 第一个文件路径
        file_b (str): 第二个文件路径
        sample_size (int): 抽样行数(默认10行)
    """
    # 读取文件A的所有行
    try:
        with open(file_a, 'r', encoding='utf-8') as f:
            lines_a = f.readlines()
    except Exception as e:
        print(f"读取文件 {file_a} 时出错: {str(e)}")
        return

    # 读取文件B的所有行
    try:
        with open(file_b, 'r', encoding='utf-8') as f:
            lines_b = f.readlines()
    except Exception as e:
        print(f"读取文件 {file_b} 时出错: {str(e)}")
        return
    
    # 检查文件行数是否一致
    len_a = len(lines_a)
    len_b = len(lines_b)
    
    if len_a != len_b:
        print(f"警告: 文件行数不一致 ({len_a} vs {len_b})")
    
    # 确定实际抽样行数（不超过最小文件行数）
    actual_sample_size = min(sample_size, min(len_a, len_b))
    
    if actual_sample_size <= 0:
        print("错误: 文件行数不足，无法抽样")
        return
    
    # 随机选择10行（或更少）
    sample_indices = random.sample(range(min(len_a, len_b)), actual_sample_size)
    sample_indices.sort()  # 按行号排序
    
    # 比较抽样行
    mismatch_count = 0
    print(f"比较结果 ({actual_sample_size} 个随机抽样):")
    print("-" * 80)
    
    for i, idx in enumerate(sample_indices):
        line_a = lines_a[idx].rstrip('\n').replace('\t', '\\t').replace('\r', '\\r')
        line_b = lines_b[idx].rstrip('\n').replace('\t', '\\t').replace('\r', '\\r') if idx < len_b else "<超出文件范围>"
        
        line_number = idx + 1  # 行号从1开始
        match = "相同" if idx < len_b and line_a == line_b else "不同"
        
        if match == "不同":
            mismatch_count += 1
            
        print(f"行号 #{line_number}: {match}")
        print(f"  {file_a}: {line_a[:100]}{'...' if len(line_a) > 100 else ''}")
        
        if idx < len_b:
            print(f"  {file_b}: {line_b[:100]}{'...' if len(line_b) > 100 else ''}")
        else:
            print(f"  {file_b}: {line_b}")
        
        print("-" * 60)
    
    # 输出总结
    print("\n比较总结:")
    print(f"  总抽样行数: {actual_sample_size}")
    print(f"  相同行数: {actual_sample_size - mismatch_count}")
    print(f"  不同行数: {mismatch_count}")
    
    if mismatch_count == 0:
        print("✅ 所有抽样行内容一致")
    else:
        print(f"❌ 发现 {mismatch_count} 处不同，建议检查完整文件")

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("使用说明: python compare_files.py <文件A路径> <文件B路径> [抽样行数]")
        print("示例: python compare_files.py a.out b.out 20")
        sys.exit(1)
    
    file_a = sys.argv[1]
    file_b = sys.argv[2]
    
    # 检查文件是否存在
    for file_path in [file_a, file_b]:
        if not os.path.exists(file_path):
            print(f"错误: 文件不存在 - {file_path}")
            sys.exit(1)
    
    # 获取抽样行数（如果提供）
    sample_size = 100
    if len(sys.argv) >= 4:
        try:
            sample_size = int(sys.argv[3])
            if sample_size < 1:
                print("警告: 抽样行数至少为1，使用默认值10")
                sample_size = 10
        except ValueError:
            print("警告: 无效的抽样行数，使用默认值10")
    
    print(f"开始比较文件:")
    print(f"  文件A: {os.path.abspath(file_a)}")
    print(f"  文件B: {os.path.abspath(file_b)}")
    print(f"  抽样行数: {sample_size}")
    print("=" * 80)
    
    compare_random_lines(file_a, file_b, sample_size)