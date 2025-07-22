#include "LogParser.hpp"
#include "TokenManager.hpp"
#include "absl/container/flat_hash_map.h"
#include "arg.hpp"
#include "internal/out.hpp"
#include "utils/util.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <processor.hpp>
#include <string>
#include <vector>
// using namespace std::chrono;
namespace chr = std::chrono;
namespace fs = std::filesystem;
void process_log_chunk(const VecS &logs, const IndexManager &im,
                       size_t chunk_idx, const Args args) {
  LogParser parser(args);
  auto start_time = chr::steady_clock::now();
  std::cout << "Processing chunk " << chunk_idx << " (" << im.len()
            << " lines)..." << std::endl;
  // 更新输出目录
  auto output_dir = args.output_dir;
  output_dir = output_dir + "/" + std::to_string(chunk_idx);
  fs::create_directories(output_dir);
  parser.update_output_dir(output_dir);
  DEBUG("parser.process_chunk: in")
  parser.process_chunk(logs, im, chunk_idx);
  DEBUG("parser.process_chunk: out")

  //////////////// ORIGINAL 0710 ////////////////

  // 使用从0开始的新计数器生成template IDs
  std::vector<uint32_t> new_tmpl_ids;
  uint32_t new_id_counter = 0;
  // 创建模板到ID的映射，用于快速查找
  absl::flat_hash_map<std::string, uint32_t> tmpl_id_map;
  std::vector<uint64_t> dynamic_entries; // 预测性分配内存
  dynamic_entries.reserve(parser.runtime_space.size() *
                          parser.runtime_space[0].first.size() / 2);

  std::vector<int32_t> tmpl_replacements;
  // 预测性分配内存 && 用完要clear防止反复分配内存
  tmpl_replacements.reserve(parser.runtime_space[0].first.size());

  for (uint32_t raw_id = 0; raw_id < parser.runtime_space.size(); raw_id++) {
    auto &runtime_entry = parser.runtime_space[raw_id];
    auto parsed_id = runtime_entry.second;
    const auto &original_tmpl = parser.parsed_log_map[parsed_id];

    // std::vector<uint64_t> tmpl_replacements; // Delete
    tmpl_replacements.clear();

    // if (!runtime_entry.empty()) {
    for (auto &var : runtime_entry.first) {
      uint64_t entry = 0;
      auto rule = parser.exp_rules_dict.find(var);
      DEBUG("var: %s, rule: %s", var.empty() ? "nullptr" : var.c_str(),
            rule == parser.exp_rules_dict.end() ? "nullptr"
                                                : rule->second.c_str())
      if (rule == parser.exp_rules_dict.end() || rule->second.empty()) {
        if (!is_numeric(var) || var.length() > 15) {
          // init_flag 只有 -1 和其它有区别
          parser.token_manager.get_or_register_token_no_split(var, 1, nullptr,
                                                              &entry);
          dynamic_entries.push_back(entry);
        }
      } else {
        // 处理都一样
        // if (rule->second.find("<>") != std::string::npos)
        parser.token_manager.get_or_register_token_no_split(rule->second, 1,
                                                            nullptr, &entry);
        tmpl_replacements.push_back(entry);
        DEBUG("tmpl_replacements.push_back: %lu", entry)
      }
    }
    // }
    // 处理模板（不管是否有替换）
    std::string tmpl_to_use = original_tmpl;
    size_t replacement_index = 0;
    size_t find_idx = tmpl_to_use.find("<->");
    while (find_idx != std::string::npos &&
           replacement_index < tmpl_replacements.size()) {
      if (tmpl_replacements[replacement_index] == -1) {
        tmpl_to_use[find_idx + 1] = '*';
      } else {
        // 将数字转换为字母标识符 (0->A, 1->B, ..., 25->Z, 26->AA, 27->AB,
        // ...)
        int32_t dict_id = tmpl_replacements[replacement_index];
        tmpl_to_use.replace(find_idx, 3, "|" + number2letter(dict_id) + "|");
      }
      ++replacement_index;
      find_idx = tmpl_to_use.find("<->", find_idx + 3);
    }
    // if (find_idx != std::string::npos) {
    //   DEBUG("fuck! parser.process_chunk: invalid template: %s",
    //         tmpl_to_use.c_str())
    //   continue;
    // }

    auto iter = tmpl_id_map.find(tmpl_to_use);
    auto tmpl_id = iter == tmpl_id_map.end() ? new_id_counter++ : iter->second;

    parser.unmapped_templates_with_dict_id.emplace(tmpl_id, tmpl_to_use);
    tmpl_id_map.emplace(tmpl_to_use, tmpl_id);
    new_tmpl_ids.push_back(tmpl_id);
  }

  SubTokenCompressor::encode_and_store_template_id(output_dir, "templateid",
                                                   new_tmpl_ids);

  auto end_time = chr::steady_clock::now();
  auto elasped = chr::duration_cast<chr::milliseconds>(end_time - start_time);
  std::cout << "Processed chunk " << chunk_idx << " in " << elasped.count()
            << "ms." << std::endl;

  parser.export_chunk_subtoken_dictionary();

  parser.export_unmapped_templates_with_dict_id_for_chunk();

  std::vector<uint8_t> buffer;
  SubTokenCompressor::batch_encode_dynamic(dynamic_entries, buffer);

  SubTokenCompressor::compress_chunk(buffer, output_dir);
}
