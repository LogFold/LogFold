#include "internal/out.hpp"
#include "utils/util.hpp"
#include <arg.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <processor.hpp>
#include <thread>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
namespace chr = std::chrono;
using namespace std;

void columnar_subtoken_compress_logs(const Args &args) {
  auto start_time = chr::steady_clock::now();

  // 递归创建目录
  fs::create_directories(args.output_dir);

  // 读取日志文件
  VecS logs;
  read_benchmark_file(logs, args.input_file);

  // 生成分块信息 (chunk_idx, start, end)
  vector<tuple<size_t, size_t, size_t>> chunks;
  for (size_t start = 0, chunk_idx = 0; start < logs.size();
       start += args.chunk_size, ++chunk_idx) {
    size_t end = min(start + args.chunk_size, logs.size());
    chunks.emplace_back(chunk_idx, start, end);

    // 打印分块信息
    cout << "Chunk " << chunk_idx << ": lines " << start << " - " << end
         << endl;
  }
  // num_threads * chunk_per_thread >= chunks.size()
  auto chunks_per_thread = chunks.size() / args.num_threads;
  // cout << "Chunks per thread: " << chunks_per_thread << endl;
  while (args.num_threads * chunks_per_thread < chunks.size())
    ++chunks_per_thread;
  cout << "Chunks per thread: " << chunks_per_thread << endl;
  vector<pair<size_t, size_t>> ranges;
  for (size_t start = 0; start < chunks.size(); start += chunks_per_thread) {
    size_t end = min(start + chunks_per_thread, chunks.size());
    ranges.emplace_back(start, end);
    cout << "Thread " << ranges.size() - 1 << ": chunks " << start << " - "
         << end << endl;
  }

  vector<thread> workers;

  for (size_t i = 0; i < args.num_threads && i < ranges.size(); ++i) {
    // auto output_dir = args.output_dir;
    workers.emplace_back([&logs, &args, &chunks, &ranges, i] {
      for (size_t j = ranges[i].first; j < ranges[i].second; ++j) {
        auto &[cidx, st, ed] = chunks[j];
        // 获取下标管理器
        IndexManager im(st, ed - st);
        // 处理块
        DEBUG("process_log_chunk: in")
        process_log_chunk(logs, im, cidx, args);
        DEBUG("process_log_chunk: out")
      }
    });
  }

  // 等待所有工作线程完成
  for (auto &worker : workers) {
    if (worker.joinable())
      worker.join();
  }

  // 打印耗时
  auto elapsed = chr::duration_cast<chr::milliseconds>(
      chr::steady_clock::now() - start_time);
  cout << "Total Processing time taken: " << elapsed.count() << "ms\n";
}

// 示例辅助函数实现 (需根据实际需求完善)
void read_benchmark_file(VecS &lines, const string &filename) {
  ifstream file(filename);
  if (!file)
    handle_error("Cannot open file: " + filename);

  string line;
  while (getline(file, line)) {
    lines.emplace_back(move(line));
  }
}
