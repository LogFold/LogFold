#include "internal/out.hpp"
#include <TokenManager.hpp>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include <utils/util.hpp>
#include <vector>
// #include <filesystem>

// namespace fs = std::filesystem;
void SubTokenCompressor::write_unsigned_leb128(std::ofstream &writer,
                                               char *buff, size_t &offset,
                                               uint64_t value) {
  do {
    uint8_t byte = value & 0x7F;
    value >>= 7;
    if (value != 0)
      byte |= 0x80;
    buff[offset++] = byte;
  } while (value != 0);

  writer.write(buff, offset);
  offset = 0;
}
void SubTokenCompressor::encode_and_store_base_binary(
    const std::string &output_dir, const uint32_t key1, const uint32_t key2,
    const std::vector<uint64_t> &vec) {

  // 1. 如果数据为空，直接返回
  if (vec.empty()) {
    return;
  }

  // 2. 构造输出路径
  std::string output_path = output_dir + "/l" + std::to_string(key1) + "_" +
                            std::to_string(key2) + ".bin";

  // 3. 打开文件写入流
  std::ofstream writer(output_path, std::ios::binary);
  if (!writer.is_open()) {
    handle_error(format(
        "encode_and_store_base_binary - Failed to create output file: %s",
        output_path.c_str()));
  }

  // 4. 抽样分析数据以决定编码策略
  size_t sample_size = std::min<size_t>(10, vec.size());
  std::vector<int64_t> sample_values(vec.begin(), vec.begin() + sample_size);
  bool is_delta_mode = calc_compression_mode(sample_values);

  char buffer[4096 + 20] = {0};
  size_t offset = 0;
  write_unsigned_leb128(writer, buffer, offset,
                        is_delta_mode ? 1 : 0); // 标记编码方式

  // 5. 根据评估结果选择编码方式

  int64_t val;
  uint8_t byte;
  bool more;
  if (is_delta_mode) {
    int64_t prev = 0;
    for (const int64_t num : vec) {
      val = num - prev;
      prev = num;
      // 内联实现有符号LEB128（避免函数调用开销）
      do {
        byte = val & 0x7F;
        val >>= 7;
        more = !((val == 0 && (byte & 0x40) == 0) ||
                 (val == -1 && (byte & 0x40) != 0));
        if (more)
          byte |= 0x80;
        buffer[offset++] = byte;
      } while (more);
      if (offset >= 4096) {
        writer.write(buffer, offset);
        offset = 0;
      }
    }
  } else {
    // 使用原始值
    for (int64_t num : vec) {
      val = num;
      do {
        byte = val & 0x7F;
        val >>= 7;
        more = !((val == 0 && (byte & 0x40) == 0) ||
                 (val == -1 && (byte & 0x40) != 0));
        if (more)
          byte |= 0x80;
        buffer[offset++] = byte;
      } while (more);
      if (offset >= 4096) {
        writer.write(buffer, offset);
        offset = 0;
      }
    }
  }
  // 最后一块数据写入文件
  if (offset > 0) {
    writer.write(buffer, offset);
  }
  writer.flush();
  writer.close();
}

void SubTokenCompressor::encode_and_store_trans_matrix_lsb(
    const std::string &output_dir, const std::string &output_name,
    const std::vector<std::vector<uint64_t>> &trans_num_matrix,
    size_t expected_row_length) {
  if (trans_num_matrix.empty()) {
    return;
  }

  // 创建输出文件
  std::string output_path = output_dir + "/" + output_name + ".bin";
  std::ofstream writer(output_path, std::ios::binary);
  if (!writer.is_open()) {
    handle_error(format(
        "encode_and_store_trans_matrix_lsb - Failed to create output file: %s",
        output_path.c_str()));
  }
  DEBUG("create output file: %s", output_path.c_str())

  // 写入行数和列数
  char buffer[4096 + 20] = {0};
  size_t offset = 0;
  write_unsigned_leb128(writer, buffer, offset, trans_num_matrix.size());
  write_unsigned_leb128(writer, buffer, offset, expected_row_length);
  DEBUG("write row num: %lu, col num: %lu", trans_num_matrix.size(),
        expected_row_length)

  bool is_delta = output_name.find("delta") != std::string::npos;

  uint8_t byte;
  bool more;

  if (is_delta) {
    for (size_t row_idx = 0; row_idx < trans_num_matrix.size(); ++row_idx) {
      const auto &row = trans_num_matrix[row_idx];
      if (row.size() != expected_row_length) {
        handle_error(format("Row %d has length %d, expected %d", row_idx,
                            row.size(), expected_row_length));
      }
      uint64_t val;
      for (uint64_t num : row) {
        val = num;
        do {
          byte = val & 0x7F;
          val >>= 7;
          if (val != 0)
            byte |= 0x80;
          buffer[offset++] = byte;
        } while (val != 0);
        if (offset >= 4096) {
          writer.write(buffer, offset);
          offset = 0;
        }
      }
    }
  } else {
    for (size_t row_idx = 0; row_idx < trans_num_matrix.size(); ++row_idx) {
      const auto &row = trans_num_matrix[row_idx];
      if (row.size() != expected_row_length) {
        handle_error(format("Row %d has length %d, expected %d", row_idx,
                            row.size(), expected_row_length));
      }

      // 取样分析是否使用 delta 编码
      size_t sample_size = std::min<size_t>(10, row.size());
      // 暂时不管 row 为空的情况
      // if (sample_size == 0) {
      //   write_unsigned_leb128(writer, 0); // 标记为原始值
      //   continue;
      // }
      std::vector<int64_t> sample_values(row.begin(),
                                         row.begin() + sample_size);
      bool is_delta_mode = calc_compression_mode(sample_values);
      write_unsigned_leb128(writer, buffer, offset,
                            is_delta_mode ? 1 : 0); // 标记编码方式
      DEBUG("write delta mode flag: %d, first ele: %lu", is_delta_mode ? 1 : 0,
            row[0])
      if (is_delta_mode) {
        // 使用 delta 编码
        int64_t val;
        int64_t prev = 0;
        for (const int64_t num : row) {
          val = num - prev;
          prev = num;
          // 内联实现有符号LEB128（避免函数调用开销）
          do {
            byte = val & 0x7F;
            val >>= 7;
            more = !((val == 0 && (byte & 0x40) == 0) ||
                     (val == -1 && (byte & 0x40) != 0));
            if (more)
              byte |= 0x80;
            buffer[offset++] = byte;
          } while (more);
          if (offset >= 4096) {
            writer.write(buffer, offset);
            offset = 0;
          }
        }
      } else {
        // 不使用 delta 编码
        uint64_t val;
        for (uint64_t num : row) {
          val = num;
          do {
            byte = val & 0x7F;
            val >>= 7;
            if (val != 0)
              byte |= 0x80;
            buffer[offset++] = byte;
          } while (val != 0);
          if (offset >= 4096) {
            writer.write(buffer, offset);
            offset = 0;
          }
        }
      }
    }
  }

  // 最后一块数据写入文件
  if (offset > 0) {
    writer.write(buffer, offset);
  }
  writer.flush();
  writer.close();
}

void SubTokenCompressor::encode_and_store_template_id(
    const std::string &output_dir, const std::string &output_name,
    const std::vector<uint32_t> &tmpl_ids) {
  std::string output_path = output_dir + "/" + output_name + ".bin";
  std::ofstream writer(output_path, std::ios::binary);
  if (!writer.is_open()) {
    handle_error(
        format("Failed to create output file: %s", output_path.c_str()));
  }
  char buffer[4096 + 20];
  size_t buffer_size = 0;
  uint64_t val;
  uint8_t byte;
  for (uint64_t num : tmpl_ids) {
    val = num;
    do {
      byte = val & 0x7F;
      val >>= 7;
      if (val != 0)
        byte |= 0x80;
      buffer[buffer_size++] = byte;
    } while (val != 0);
    if (buffer_size >= 4096) {
      writer.write(buffer, buffer_size);
      buffer_size = 0;
    }
  }
  if (buffer_size > 0) {
    writer.write(buffer, buffer_size);
  }
  writer.flush();
  writer.close();
}

bool SubTokenCompressor::calc_compression_mode(std::vector<int64_t> &nums) {
  if (nums.empty())
    return false;
  // 处理第一个元素
  int64_t prev = nums[0];
  int64_t sum_abs_original = std::abs(prev);
  int64_t sum_abs_delta = std::abs(prev); // 第一个元素的delta就是自身

  // 从第二个元素开始处理
  for (size_t i = 1; i < nums.size(); ++i) {
    int64_t num = nums[i];
    sum_abs_original += std::abs(num);
    sum_abs_delta += std::abs(num - prev);
    prev = num;
  }

  // 比较平均值（用乘法避免浮点除法）
  return sum_abs_delta < sum_abs_original;
}

std::vector<int64_t>
SubTokenCompressor::compute_delta_values(const std::vector<uint64_t> &nums) {
  size_t len = nums.size();
  std::vector<int64_t> deltas(len);
  int64_t prev = deltas[0] = nums[0];
  for (size_t i = 1; i < len; ++i) {
    deltas[i] = int64_t(nums[i]) - prev;
    prev = nums[i];
  }
  return deltas;
}

void SubTokenCompressor::batch_encode_dynamic(
    const std::vector<uint64_t> &dynamic, std::vector<uint8_t> &buffer) {
  buffer.reserve(dynamic.size() * 5);
  uint8_t byte;
  for (uint64_t val : dynamic) {
    do {
      byte = val & 0x7F;
      val >>= 7;
      if (val != 0)
        byte |= 0x80;
      buffer.push_back(byte);
    } while (val != 0);
  }
}

void SubTokenCompressor::compress_chunk(const std::vector<uint8_t> &buffer,
                                        const std::string &output_path) {
  std::cout << "Compressing data to file: " << output_path << std::endl;
  auto binary_file_path = output_path + "/tokenid.bin";

  std::ofstream writer(binary_file_path, std::ios::binary);
  if (!writer.is_open()) {
    handle_error(
        format("Failed to create output file: %s", binary_file_path.c_str()));
  }
  writer.write(reinterpret_cast<const char *>(buffer.data()), buffer.size());
  writer.flush();
  writer.close();

  auto xz_file_path = output_path + ".tar.xz";

  std::vector<const char *> args = {
      "tar",
      "-cf",
      xz_file_path.c_str(),
      "--use-compress-program=xz --extreme",
      "-C",
      output_path.c_str(),
      ".",
      nullptr // 参数列表必须以 nullptr 结束
  };

  int stderr_pipe[2];
  if (pipe(stderr_pipe)) {
    handle_error("Failed to create pipe");
  }

  pid_t pid = fork();
  if (pid == -1) {
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    handle_error("fork failed");
  }

  if (pid == 0) { // 子进程
    // 重定向 stderr 到管道
    dup2(stderr_pipe[1], STDERR_FILENO);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);

    // 执行 tar 命令（注意：const_cast 因 execvp 需要 char* const*）
    execvp("tar", const_cast<char *const *>(args.data()));

    // 如果执行到这里说明 execvp 失败
    std::cerr << "execvp failed" << std::endl;
    _exit(EXIT_FAILURE);
  } else {                 // 父进程
    close(stderr_pipe[1]); // 关闭写端

    // 读取子进程的错误输出
    std::ostringstream error_stream;
    char buffer[256];

    ssize_t count;
    while ((count = read(stderr_pipe[0], buffer, sizeof(buffer))) > 0) {
      error_stream.write(buffer, count);
    }
    close(stderr_pipe[0]);

    // 等待子进程结束
    int status;
    if (waitpid(pid, &status, 0) == -1) {
      handle_error("waitpid failed");
    }

    // 检查命令是否成功
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      std::string error_msg = "tar command failed: " + error_stream.str();
      handle_error(error_msg);
    }
  }
}