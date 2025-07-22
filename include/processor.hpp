#ifndef LOGMD_PROCESSOR_HPP
#define LOGMD_PROCESSOR_HPP

#include "LogParser.hpp"
#include "arg.hpp"
#include <cstddef>
#include <string>
#include <vector>

void columnar_subtoken_compress_logs(const Args &args);

// 处理日志块的函数
void process_log_chunk(const VecS &logs, const IndexManager &im,
                       size_t chunk_idx, const Args args);

// 读取文件函数
void read_benchmark_file(VecS &lines, const std::string &filename);

#endif // LOGMD_PROCESSOR_HPP