#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/hash/hash.h"
#include "arg.hpp"
#include "internal/out.hpp"
#include "typedef.hpp"
#include "utils/Array2.hpp"
#include "utils/IndexSet.hpp"
#include "utils/fpgrow.hpp"
#include "utils/pcre2regex.hpp"
#include <LogParser.hpp>
#include <algorithm>
#include <cmath>
#include <constant.hpp>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <utils/util.hpp>
#include <vector>
using namespace std;

LogParser::LogParser(const Args &args)
    : args(args), MAIN_TOKEN_RE(R"(([^\s|]+)|(\s)|(\|))"),
      DYNAMIC_TOKEN_RE(R"(([\p{L}\p{N}]+)|([^\p{L}\p{N}]+))") {
  CLASSIFY_PATTERNS.emplace_back(R"((/[^/ ]*)+|([a-zA-Z]:\\(?:[^\\ ]*\\)*))");
  CLASSIFY_PATTERNS.emplace_back(R"(^\S*\d\S*$)");
}
void LogParser::update_output_dir(const string &output_dir) {
  this->output_dir = output_dir;
}

void LogParser::process_chunk(const VecS &chunk, const IndexManager &im,
                              size_t chunk_idx) {

  for (size_t i = 0; i < im.len(); i++) {
    parse_one(chunk[im[i]]);
  }

  token_manager.process_base_dict_for_vec(output_dir);

  DEBUG("pasrser.process_patterns_exp: in")
  process_patterns_exp();
  DEBUG("pasrser.process_patterns_exp: out")

  DEBUG("pasrser.process_matrix_ndarray_dict: in")
  process_matrix_ndarray_dict();
  DEBUG("pasrser.process_matrix_ndarray_dict: out")

  DEBUG("token_manager.process_simple_var_dict: in")
  token_manager.process_simple_var_dict();
  DEBUG("token_manager.process_simple_var_dict: out")
}

void LogParser::parse_one(const string &log) {
  // size_t raw_id = raw_id_counter++;

  // Call parse_template which now returns all processed information

  // Update runtime space
  VecS total_dynamic_vars;
  string templ;
  bool res =
      parse_template_and_process_dynamic_vars(log, templ, total_dynamic_vars);
  if (!res) {
    return;
  }
  // Get parsed_id and update template index
  uint32_t parsed_id = manage_template(templ);
  runtime_space.emplace_back(move(total_dynamic_vars), parsed_id);
  // Update mappings
  // raw_to_parsed_map.push_back(parsed_id);
}

// bool LogParser::parse_template(const string &log, uint32_t &parsed_id,
//                                VecS &total_dynamic_vars) {

//   // Returns (parsed_id, statement_id, statement_idx, sequence_id,
//   // total_dynamic_vars, dynamic_vecs)

//   // Step 1: Call the refactored function to extract template and process
//   // dynamic variables in one go
//   string templ;
//   bool res =
//       parse_template_and_process_dynamic_vars(log, templ,
//       total_dynamic_vars);
//   if (!res) {
//     return false;
//   }
//   // Generate parsed_id and update template index
//   parsed_id = manage_template(templ);
//   return true;
// }

bool LogParser::parse_template_and_process_dynamic_vars(
    const string &log, string &templ, VecS &total_dynamic_vars) {
  if (log.empty()) {
    return false;
  }

  VecS template_parts;
  size_t total_len = 0;
  // 使用正则表达式匹配所有 token
  for (auto it = MAIN_TOKEN_RE.get_iter(log); !it.end(); it.next()) {
    auto [start, end] = it.cap();
    total_len += classify_and_process_token(log.substr(start, end - start),
                                            template_parts, total_dynamic_vars);
  }


  templ.reserve(total_len);
  for (const auto &part : template_parts) {
    templ += part;
  }
  return true;
}

size_t LogParser::classify_and_process_token(string token, VecS &template_parts,
                                             VecS &total_dynamic_vars) {

  static const VecS keys = {"<a>", "<b>", "<c>", "<d>", "<e>", "<f>",
                            "<g>", "<h>", "<i>", "<j>", "<k>", "<l>",
                            "<m>", "<n>", "<o>", "<*>", "<->"};

  int placeholder_index = -1;

  for (const auto &pat_re : CLASSIFY_PATTERNS) {
    if (pat_re.match(token)) {


      if (is_numeric(token)) {
        if (token.length() <= 15) {

          placeholder_index = token.length() - 1;


          token_manager.get_or_register_token_no_split(token, -1, nullptr,
                                                       nullptr);
        } else {
          placeholder_index = 15;
          token_manager.simple_var_dict.insert(token);
        }
      } else {
        bool contains_punctuation = false;
        bool has_alnum = false;
        for (auto c : token) {
          if (contains_punctuation && has_alnum) {
            break;
          } else if (!isalnum(c)) {
            contains_punctuation = true;
          } else {
            has_alnum = true;
          }
        }

        if (!contains_punctuation) {
          placeholder_index = 15;
          token_manager.simple_var_dict.insert(token);
        } else if (has_alnum) {

            // string non_punctuation_chars;
            // std::copy_if(token.begin(), token.end(),
            //              std::back_inserter(non_punctuation_chars),
            //              [](char c) { return std::isalnum(c); });

          placeholder_index = 16;
          // 处理动态 token
          // auto token_vec =
          process_dynamic_token(token);
          // if (!token_vec.empty()) {
          //   dynamic_vecs.push_back(std::move(token_vec));
          // } else {
          //   dynamic_vecs.emplace_back();
          // }
        } else {
          // 纯符号 token
          placeholder_index = -1;
          // cout << "IN CLASSIFY_AND_PROCESS_TOKEN: token: " << token << endl;
        }
      }
      break;
    }
  }

  if (placeholder_index >= 0) {
    template_parts.push_back(keys[placeholder_index]);
    total_dynamic_vars.emplace_back(move(token));
    return 3;
  } else {
    size_t l = token.length();
    template_parts.emplace_back(move(token));
    return l;
  }
}

bool LogParser::process_dynamic_token(const string &token) {

  if (token.empty()) {
    return false;
  }

  // static const regex DYNAMIC_TOKEN_RE(R"(([\p{L}\p{N}]+)|([^\p{L}\p{N}]+))");

  string pattern;
  pattern.reserve(token.size() + 10);
  VecS parts;
  parts.reserve(8);
  int word_index = 0;

  //这里是在分词，每个m就是一个sub-token
  for (auto match = DYNAMIC_TOKEN_RE.get_iter(token); !match.end();
       match.next()) {
    //分组一字母+数字
    if (match.has_cap(1)) {
      auto [st, ed] = match.cap(1);
      auto word =
          token.substr(st, ed - st) + "(" + to_string(word_index++) + ")";
      // token_manager.token_frequency[word]++;
      parts.emplace_back(move(word));
      pattern += "<>";
      // cout << "cap 1: " << word << endl;
    }
    // 分组二 非字母非数字（如符号）
    else if (match.has_cap(2)) {
      auto [st, ed] = match.cap(2);
      pattern += token.substr(st, ed - st);
      // cout << "cap 2: " << token.substr(st, ed - st) << endl;
    } else {
      // cout << match.has_cap() << endl;
      handle_error(
          format("IN PROCESS_DYNAMIC_TOKEN - ERROR in unknown group, token: %s",
                 token.c_str()));
    }
  }

  VecS result;
  result.reserve(parts.size() + 1);
  result.emplace_back(move(pattern));
  for (int i = 0; i < parts.size(); i++)
    result.emplace_back(move(parts[i]));

  if (result.size() <= 1) {
    string error_msg = result.empty()
                           ? "IN PROCESS_DYNAMIC_TOKEN - RESULT: <nil>"
                           : "IN PROCESS_DYNAMIC_TOKEN - RESULT: " + result[0];
    handle_error(error_msg);
  }

  // Update dictionaries
  auto &container = sole_pat_dict[result[0]];
  container.set.insert(result);
  container.vec.emplace_back(result);
  return true;
}

uint32_t LogParser::manage_template(const string &templ) {
  auto it = template_index.find(templ);
  if (it != template_index.end()) {
    return it->second;
  }
  uint32_t id = parsed_id_counter++;
  parsed_log_map.emplace(id, templ);
  template_index.emplace(templ, id);
  return id;
}

void LogParser::process_patterns_exp() {
  auto &sole_pat = sole_pat_dict.to_vector();
  size_t len = sole_pat.size();
  for (size_t i = 0; i < len; i++) {
    auto &sole_pat_key = sole_pat[i].first;
    auto &sole_pat_vec = sole_pat[i].second.vec;
    auto &sole_pat_set = sole_pat[i].second.set;

    if (sole_pat_vec.empty()) {
      continue;
    } else if (sole_pat_vec.size() == 1) {
      // 直接合成注册
      string token;
      generate_rules(sole_pat_vec[0], token);
      exp_rules_dict.emplace(token, token);
      token_manager.simple_var_dict.emplace(move(token));
      continue;
    }
    DEBUG("sole_pat_vec = (%lu, %lu)", sole_pat_vec.size(),
          sole_pat_vec[0].size());
    Array2<string> arr(&sole_pat_vec);

    // =========================================================================
    //  第一阶段：分析每个槽位并收集统计数据
    // =========================================================================
    DEBUG("first phase")
    vector<SlotStats> slot_stat_vec;
    slot_stat_vec.reserve(arr.get_col_count() - 1);
    VecS update_pat_vec = {sole_pat_key};
    vector<size_t> keep_column_idx;
    size_t all_rows = arr.get_row_count();

    for (size_t col_idx = 1; col_idx < arr.get_col_count(); ++col_idx) {

      const auto &column_index = arr.get_column(col_idx);
      // 统计频次
      map<string, uint32_t> counts_map;
      for (size_t i = 0; i < column_index.size(); ++i) {
        counts_map[arr[column_index.get(i)]] += 1;
      }

      if (counts_map.size() == 1) {
        // 唯一值，加入 update_pat_vec
        update_pat_vec.push_back(arr[column_index.get(0)]);
        continue;
      } else {
        keep_column_idx.push_back(col_idx);
      }

      auto &slot_stats = slot_stat_vec.emplace_back();
      slot_stats.column_index = col_idx;
      slot_stats.unique_values_count = counts_map.size();

      // 自动计算超参数
      // size_t min_peak_height = rows / slot_stats.unique_values_count;
      // size_t min_peak_distance = 1;

      // 找出代表性值
      uint32_t rep_sum = 0;
      find_representative_values(counts_map, all_rows,
                                 slot_stats.representative_values, rep_sum);

      // 计算优势比
      slot_stats.dominance_ratio = double(rep_sum) / all_rows;

      // 计算信息熵
      slot_stats.entropy = calculate_entropy(counts_map, all_rows);
    }

    // =========================================================================
    //  第二阶段：选择最佳标志位
    // =========================================================================
    DEBUG("second phase")
    if (slot_stat_vec.empty()) {
      string final_pat_key;
      generate_rules(update_pat_vec, final_pat_key);
      add_to_exp_rules_dict(sole_pat_set, final_pat_key);
      token_manager.simple_var_dict.emplace(move(final_pat_key));

      continue;

    } else if (slot_stat_vec.size() == 1) {
      string final_pat_key;
      generate_rules(update_pat_vec, final_pat_key);
      add_to_exp_rules_dict(sole_pat_set, final_pat_key);
      add_to_matrix_ndarray_dict(update_pat_vec, sole_pat_set, keep_column_idx,
                                 final_pat_key, arr);
      continue;
    }

    sort(slot_stat_vec.begin(), slot_stat_vec.end(),
         [](const SlotStats &a, const SlotStats &b) {
           if (a.unique_values_count != b.unique_values_count) {
             return a.unique_values_count <
                    b.unique_values_count; // 按唯一值数量升序排序
           }

           if (a.dominance_ratio != b.dominance_ratio) {
             return a.dominance_ratio > b.dominance_ratio; // 按优势比降序排序
           }

           return a.entropy < b.entropy; // 按熵升序排序
         });

    auto &best_slot_stats = slot_stat_vec[0];
    DEBUG("best_slot_stats: column_index=%lu, unique_values_count=%lu",
          best_slot_stats.column_index, best_slot_stats.unique_values_count)

    // =========================================================================
    //  第三阶段：基于最佳标志位进行分类和输出
    // =========================================================================
    DEBUG("third phase")
    auto best_column_index = arr.get_column(best_slot_stats.column_index);
    VecS new_patterns, new_pat_keys;

    if (best_slot_stats.representative_values.empty() ||
        best_slot_stats.representative_values.size() >=
            args.rep_val_threshold ||
        best_slot_stats.dominance_ratio <= args.dom_ratio) {
      DEBUG("best representative_values.size=%lu, dominance_ratio=%lf; "
            "update_pat_vec.size=%lu",
            best_slot_stats.representative_values.size(),
            best_slot_stats.dominance_ratio, update_pat_vec.size())
      string final_pat_key;
      generate_rules(update_pat_vec, final_pat_key);
      add_to_exp_rules_dict(sole_pat_set, final_pat_key);
      add_to_matrix_ndarray_dict(update_pat_vec, sole_pat_set, keep_column_idx,
                                 final_pat_key, arr);
      continue;
    }
    DEBUG("best representative values is not empty")

    uint32_t new_posotion = best_slot_stats.column_index;
    string mined_pat_key;
    generate_rules(update_pat_vec, mined_pat_key);
    bool is_unique_values_count_less_than_x =
        best_slot_stats.unique_values_count <= args.zeta;

    DEBUG("is_unique_values_count_less_than_x: %s",
          is_unique_values_count_less_than_x ? "true" : "false")
    if (is_unique_values_count_less_than_x) {
      IndexSet<string> all_unique_set;
      for (const auto &it : best_slot_stats.representative_values)
        all_unique_set.insert(it);

      // 处理离群点数据
      for (size_t i = 0; i < best_column_index.size(); ++i) {
        const auto &point = arr[best_column_index.get(i)];
        if (!all_unique_set.contains(point)) {
          all_unique_set.insert(point);
        }
      }

      const auto &all_unique_values = all_unique_set.to_vector();

      add_to_new_patterns(update_pat_vec, all_unique_values, new_patterns,
                          new_pat_keys);

    } else {
      add_to_new_patterns(update_pat_vec, best_slot_stats.representative_values,
                          new_patterns, new_pat_keys);
    }

    DEBUG("get missing matrix")
    //先根据keep_column_idx来获得missing_matrix
    auto missing_matrix = arr.get_columns(keep_column_idx);

    //然后根据new_pat_keys来继续划分missing_matrix_for_final_pat_key,具体来说就是将检查new_position对应的那列的值，如果值在new_pat_keys中，则将该行保存到新的missing_matrix中
    // target_column_idx是new_position在keep_column_idx中的位置
    auto target_column_idx = arr.get_column(new_posotion);
    // size_t all_rows = arr.get_row_count();
    absl::flat_hash_set<size_t> rows_to_delete;

    // 要区分出哪些行是对应new_pat_keys的
    for (size_t index = 0; index < new_pat_keys.size(); index++) {
      auto &new_pat = new_pat_keys[index];
      vector<size_t> filtered_rows;
      //
      for (size_t j = 0; j < target_column_idx.size(); j++) {
        if (arr[target_column_idx.get(j)] == new_pat) {
          rows_to_delete.insert(j);
          filtered_rows.push_back(j);
        }
      }

      if (!filtered_rows.empty()) {
        DEBUG("filtered_rows size: %lu", filtered_rows.size())
        vector<size_t> new_keep_column_idx(keep_column_idx.size() - 1);
        for (size_t j = 0, k = 0; j < keep_column_idx.size(); j++) {
          if (keep_column_idx[j] == new_posotion)
            continue;
          new_keep_column_idx[k++] = keep_column_idx[j];
        }

        // 形状应该为 filtered_rows.size() * (keep_column_idx.size() - 1)
        auto new_missing_matrix = arr.get_columns(new_keep_column_idx);
        new_missing_matrix.set_row_index(filtered_rows);
        auto row_len = new_missing_matrix.get_row_count(),
             col_len = new_missing_matrix.get_col_count();

        // 可优化 TODO
        DEBUG("row_len: %lu, col_len: %lu", row_len, col_len)
        absl::flat_hash_set<VecS> new_missing_matrix_hashset;
        for (size_t j = 0; j < row_len; j++) {
          VecS tmp_vec;
          tmp_vec.reserve(col_len);
          for (size_t k = 0; k < col_len; k++) {
            tmp_vec.push_back(arr[new_missing_matrix.get(j, k)]);
          }
          new_missing_matrix_hashset.emplace(move(tmp_vec));
        }

        size_t min_support = new_missing_matrix_hashset.size();
        auto new_pattern = new_patterns[index];
        vector<shared_ptr<string>> updated_fp_vec;

        if (col_len < 15) {
          // 构建matrix_vec
          vector<vector<shared_ptr<string>>> matrix_vec;
          matrix_vec.reserve(new_missing_matrix_hashset.size());
          for (auto &vec : new_missing_matrix_hashset) {
            vector<shared_ptr<string>> row_vec;
            row_vec.reserve(vec.size());
            for (auto &item : vec) {
              row_vec.push_back(make_shared<string>(item));
            }
            matrix_vec.emplace_back(move(row_vec));
          }
          // fp_growth挖掘
          DEBUG("start fp_growth")
          FPGrowth fp_growth(move(matrix_vec), min_support);
          auto patterns = fp_growth.run();
          DEBUG("fp_growth end, patterns size: %lu", patterns.size())
          if (!patterns.empty()) {
            size_t best_pat_idx = 0;
            for (int i = 1; i < patterns.size(); i++) {
              if (patterns[i].first.size() >
                  patterns[best_pat_idx].first.size())
                best_pat_idx = i;
            }
            auto &fp_new_pat = patterns[best_pat_idx].first;
            auto new_pat_vec = update_pat_vec;
            new_pat_vec.reserve(new_pat_vec.size() + fp_new_pat.size() + 1);
            new_pat_vec.push_back(new_pat);
            for (auto item : fp_new_pat) {
              DEBUG("pattern: %s", item->c_str())
              new_pat_vec.push_back(*item);
            }
            updated_fp_vec = move(fp_new_pat);
            generate_rules(new_pat_vec, new_pattern);
          }
        }

        auto tmp_vec = update_pat_vec;
        tmp_vec.reserve(tmp_vec.size() + updated_fp_vec.size() + 1);
        for (auto item : updated_fp_vec) {
          tmp_vec.push_back(*item);
        }
        tmp_vec.push_back(new_pat);
        size_t tmp_size = tmp_vec.size();
        DEBUG("tmp_vec size: %lu", tmp_size)
        for (auto &token : new_missing_matrix_hashset) {
          copy(token.begin(), token.end(), back_inserter(tmp_vec));
          string token_str;
          generate_rules(tmp_vec, token_str);
          exp_rules_dict.emplace(move(token_str), new_pattern);
          tmp_vec.resize(tmp_size);
        }

        //接下来需要修改，因为new_pattern已经变了
        //注册pattern-id
        DEBUG("new_pattern: %s", new_pattern.c_str())
        uint64_t dict_id;
        token_manager.get_or_register_token_no_split(new_pattern, 0, nullptr,
                                                     &dict_id);

        vector<bool> has_leading_zero_or_big_number_vec;
        vector<bool> is_numeric_vec;
        vector<int32_t> position_length;
        absl::flat_hash_set<size_t> columns_to_remove;
        has_leading_zero_or_big_number_vec.reserve(col_len);
        is_numeric_vec.reserve(col_len);
        position_length.reserve(col_len);

        DEBUG("for new_missing_matrix do ... col_len=%lu", col_len)
        //遍历missing_matrix_for_final_pat_key的每一列，获得每一列的unique的值的个数
        for (size_t col_idx = 0; col_idx < col_len; ++col_idx) {
          DEBUG("loop col_idx: %lu", col_idx)
          IndexSet<string> unique_set;
          for (size_t i = 0; i < row_len; ++i)
            unique_set.insert(arr[new_missing_matrix.get(i, col_idx)]);
          auto &unique_values = unique_set.to_vector();

          DEBUG("unique_values size: %lu, update_pat_vec size: %lu",
                unique_values.size(), update_pat_vec.size())

          if (!updated_fp_vec.empty()) {
            bool should_remove_column = false;
            for (auto ele : updated_fp_vec) {
              if (unique_set.contains(*ele)) {
                should_remove_column = true;
                break;
              }
            }
            if (should_remove_column) {
              // new_keep_column_idx的index
              DEBUG("should remove column: %lu", col_idx)
              columns_to_remove.insert(col_idx);
              continue;
            }
          }

          absl::flat_hash_set<size_t> length_vec;
          bool is_num = true;
          bool has_leading_zero_or_big_number = false;
          DEBUG("for unique_values do ...")
          for (const auto &ele : unique_values) {
            string pur_ele = ele.substr(0, ele.find('('));

            if (is_num && !is_numeric(pur_ele)) {
              is_num = false;
            }

            size_t len = pur_ele.size();
            length_vec.insert(len);

            if (is_num && (len > 15 || (len > 1 && pur_ele[0] == '0'))) {
              has_leading_zero_or_big_number = true;
            }
          }

          is_numeric_vec.push_back(is_num);
          has_leading_zero_or_big_number_vec.push_back(
              has_leading_zero_or_big_number);

          if (length_vec.size() == 1) {
            DEBUG("length: %lu", *length_vec.begin())
            position_length.push_back(*length_vec.begin());
          } else {
            position_length.push_back(-1);
          }
        }

        DEBUG("columns_to_remove size: %lu", columns_to_remove.size())
        auto columns_to_keep =
            new_missing_matrix.get_columns_to_keep(columns_to_remove);
        DEBUG("columns_to_keep size: %lu", columns_to_keep.size())

        if (!columns_to_keep.empty()) {
          DEBUG("add to matrix_ndarray_dict")
          // 取得matrix_ndarray的别名再进行修改
          auto &matirx_ndarray = token_manager.matrix_ndarray_dict[dict_id];
          matirx_ndarray.has_leading_zero_or_big_number_vec =
              move(has_leading_zero_or_big_number_vec);
          matirx_ndarray.is_numeric_vec = move(is_numeric_vec);
          matirx_ndarray.position_length = move(position_length);
          matirx_ndarray.arr = arr;
          if (!updated_fp_vec.empty()) {
            new_missing_matrix.set_col_index_by_swap(columns_to_keep);
          }
          matirx_ndarray.arr_idx = move(new_missing_matrix);
        }
      } else {
        handle_error("No rows matched the criteria.");
      }
    }

    if (!rows_to_delete.empty()) {
      DEBUG("rows_to_delete size: %lu, all_rows: %lu", rows_to_delete.size(),
            all_rows)
      if (rows_to_delete.size() >= all_rows)
        continue;

      vector<size_t> rows_to_keep;
      rows_to_keep.reserve(all_rows - rows_to_delete.size());
      for (int i = 0; i < all_rows; i++) {
        if (!rows_to_delete.contains(i))
          rows_to_keep.push_back(i);
      }

      missing_matrix.set_row_index_by_swap(rows_to_keep);

      uint64_t dict_id;
      token_manager.get_or_register_token_no_split(mined_pat_key, 0, nullptr,
                                                   &dict_id);

      // 获得missing_matrix
      // auto missing_matrix_for_final_pat_key =
      // arr.get_columns(keep_column_idx);

      // 取得matrix_ndarray的别名再进行修改
      auto &matirx_ndarray = token_manager.matrix_ndarray_dict[dict_id];
      matirx_ndarray.has_leading_zero_or_big_number_vec.reserve(
          keep_column_idx.size());
      matirx_ndarray.is_numeric_vec.reserve(keep_column_idx.size());
      matirx_ndarray.position_length.reserve(keep_column_idx.size());
      matirx_ndarray.arr = arr;

      //遍历missing_matrix_for_final_pat_key的每一列，获得每一列的unique的值的个数
      for (size_t col_idx = 0; col_idx < missing_matrix.get_col_count();
           ++col_idx) {
        absl::flat_hash_set<size_t> length_vec;
        bool is_num = true;
        bool has_leading_zero_or_big_number = false;

        IndexSet<string> unique_set;
        for (size_t i = 0; i < missing_matrix.get_row_count(); ++i)
          unique_set.insert(arr[missing_matrix.get(i, col_idx)]);
        auto &unique_values = unique_set.to_vector();

        for (const auto &ele : unique_values) {
          string pur_ele = ele.substr(0, ele.find('('));
          size_t len = pur_ele.size();
          length_vec.insert(len);
          if (is_num && !is_numeric(pur_ele)) {
            is_num = false;
          }
          if (is_num && (len > 15 || (len > 1 && pur_ele[0] == '0'))) {
            has_leading_zero_or_big_number = true;
          }
        }

        matirx_ndarray.is_numeric_vec.push_back(is_num);
        matirx_ndarray.has_leading_zero_or_big_number_vec.push_back(
            has_leading_zero_or_big_number);

        if (length_vec.size() == 1) {
          matirx_ndarray.position_length.push_back(*length_vec.begin());
        } else {
          matirx_ndarray.position_length.push_back(-1);
        }
      }

      size_t row_len = missing_matrix.get_row_count(),
             col_len = missing_matrix.get_col_count();
      absl::flat_hash_set<VecS> missing_matrix_hashset;
      for (size_t j = 0; j < row_len; j++) {
        VecS tmp_vec;
        tmp_vec.reserve(col_len);
        for (size_t k = 0; k < col_len; k++) {
          tmp_vec.push_back(arr[missing_matrix.get(j, k)]);
        }
        missing_matrix_hashset.emplace(move(tmp_vec));
      }
      auto token_vec = update_pat_vec;
      for (auto &token : missing_matrix_hashset) {
        copy(token.begin(), token.end(), back_inserter(token_vec));
        string token_str;
        generate_rules(token_vec, token_str);
        exp_rules_dict.emplace(move(token_str), mined_pat_key);
        token_vec.resize(update_pat_vec.size());
      }

      matirx_ndarray.arr_idx = move(missing_matrix);

    } else {
      if (!is_unique_values_count_less_than_x)
        handle_error(
            "rows_to_delete is empty, shall only when unique number <= 5");
    }
  }
}

// 从项集中根据标记的索引生成规则
void LogParser::generate_rules(const VecS &itemset, string &rule) {
  if (itemset.empty()) {
    return;
  }
  // 查找包含"<>"的元素作为模板
  int template_idx = -1;
  for (size_t i = 0; i < itemset.size(); ++i) {
    if (itemset[i].find("<>") != string::npos) {
      template_idx = i;
      break;
    }
  }

  if (template_idx == -1)
    return;
  auto &template_str = itemset[template_idx];

  // 构造位置映射表
  absl::flat_hash_map<size_t, string> position_map;
  for (size_t i = 0; i < itemset.size(); ++i) {
    if (i == template_idx)
      continue;
    auto pos_start = itemset[i].find('(');
    auto pos_end = itemset[i].rfind(')');
    if (pos_end != string::npos && pos_start < pos_end) {
      uint32_t position;
      if (!try_stoul(itemset[i].substr(pos_start + 1, pos_end - pos_start - 1),
                     position))
        continue;
      position_map.emplace(position, itemset[i].substr(0, pos_start));
    }
  }

  if (position_map.empty()) {
    rule = template_str;
    return;
  }

  rule.clear();
  rule.reserve(template_str.length() * 2);
  size_t last_end = 0;
  size_t placeholder_idx = 0;
  size_t absolute_start = template_str.find("<>", last_end);

  // 一次扫描模板，替换所有占位符
  while (absolute_start != std::string::npos) {
    // 复制占位符前的部分
    rule += template_str.substr(last_end, absolute_start - last_end);

    if (position_map.count(placeholder_idx)) {
      rule += position_map[placeholder_idx];
    } else {
      rule += "<>";
    }

    last_end = absolute_start + 2;
    ++placeholder_idx;
    absolute_start = template_str.find("<>", last_end);
  }

  if (last_end < template_str.length())
    rule += template_str.substr(last_end);
}

void LogParser::find_representative_values(
    const map<string, uint32_t> &counts_map, const uint32_t rows, VecS &values,
    uint32_t &rep_sum) {

  // 1. 自动计算超参数
  // 注意：这里使用浮点数除法避免整数除法结果为0
  uint32_t mean_peak_height = rows / counts_map.size();

  //如果超参数=1，则应该跳过，所有都是异常点
  if (mean_peak_height == 1) {
    return;
  }
  // uint32_t mean_peak_distance = 1; // Allow adjacent peaks

  // 2. 准备数据并寻找峰值
  vector<pair<string, uint32_t>> counts_vector;
  counts_vector.reserve(counts_map.size());
  for (const auto &item : counts_map) {
    counts_vector.push_back(item);
  }

  values.reserve(counts_vector.size() / 2);
  for (size_t i = 0; i < counts_vector.size(); ++i) {
    if (counts_vector[i].second < mean_peak_height)
      continue;
    bool is_left_max =
        i == 0 || counts_vector[i].second >= counts_vector[i - 1].second;
    bool is_right_max = i == counts_vector.size() - 1 ||
                        counts_vector[i].second >= counts_vector[i + 1].second;
    if (is_left_max && is_right_max) {
      values.push_back(counts_vector[i].first);
      rep_sum += counts_vector[i].second;
    }
  }

  // 如果 distance <= 1，不需要抑制，直接返回候选值
  // if (mean_peak_distance <= 1) {
  // }

  // 按照峰值大小排序（从高到低）
  // sort(candidates.begin(), candidates.end(), [&](uint32_t a, uint32_t b) {
  //   return counts_vector[a].second > counts_vector[b].second;
  // });

  // vector<bool> suppressed(rows, false);
  // vector<uint32_t> finalPeaks;

  // for (uint32_t peakIdx : candidates) {
  //   if (!suppressed[peakIdx]) {
  //     finalPeaks.push_back(peakIdx);
  //     uint32_t start =
  //         (peakIdx > mean_peak_distance) ? peakIdx - mean_peak_distance :
  //         0;
  //     uint32_t end = min(peakIdx + mean_peak_distance, rows - 1);

  //     for (uint32_t i = start; i <= end; ++i) {
  //       suppressed[i] = true;
  //     }
  //   }
  // }

  // 最终按顺序排序
  // std::sort(finalPeaks.begin(), finalPeaks.end());
}

double LogParser::calculate_entropy(const map<string, uint32_t> &counts_map,
                                    const uint32_t rows) {
  if (rows == 0) {
    return 0.0;
  }

  double totalF = static_cast<double>(rows);
  double entropy = 0.0;

  for (const auto &item : counts_map) {
    size_t count = item.second;
    if (count > 0) {
      double p = static_cast<double>(count) / totalF;
      entropy -= p * log2(p); // H(X) = - Σ [ p(x) * log₂(p(x)) ]
    }
  }

  return entropy;
}

void LogParser::process_matrix_ndarray_dict() {
  auto &matrix_ndarray_dict = token_manager.matrix_ndarray_dict.to_vector();
  for (auto &[dict_id, matirx_ndarray] : matrix_ndarray_dict) {
    process_single_matrix(dict_id, matirx_ndarray);
  }
}

void LogParser::process_single_matrix(uint64_t dict_id,
                                      MatrixNdarray &matirx_ndarray) {
  if (should_use_delta_optimization(matirx_ndarray)) {
    DEBUG("parser.process_matrix_with_delta_optimization in")
    process_matrix_with_delta_optimization(dict_id, matirx_ndarray);
    DEBUG("parser.process_matrix_with_delta_optimization out")
  } else {
    DEBUG("parser.process_single_matrix_original in")
    process_single_matrix_original(dict_id, matirx_ndarray);
    DEBUG("parser.process_single_matrix_original out")
  }
}

void LogParser::process_matrix_with_delta_optimization(
    uint64_t dict_id, MatrixNdarray &matirx_ndarray) {
  auto &arr = matirx_ndarray.arr;
  auto &arr_idx = matirx_ndarray.arr_idx;
  auto &length = matirx_ndarray.position_length;

  vector<vector<uint64_t>> result_data;
  vector<uint64_t> combined_numbers;

  DEBUG("Step 1: Combine columns into numbers")
  // Step 1: Combine columns into numbers
  string combined_str;
  for (size_t row_idx = 0; row_idx < arr_idx.get_row_count(); ++row_idx) {
    for (size_t col_idx = 0; col_idx < arr_idx.get_col_count(); ++col_idx) {
      auto &ele = arr[arr_idx.get(row_idx, col_idx)];
      string clean_value = ele.substr(0, ele.find('('));
      size_t expected_len = length[col_idx];
      // 填充到指定长度
      combined_str += string(expected_len - clean_value.length(), '0');
      combined_str += clean_value;
    }

    uint64_t combined_num = 0;
    if (try_stoull(combined_str, combined_num)) {
      combined_numbers.push_back(combined_num);
    } else {
      process_single_matrix_original(dict_id, matirx_ndarray);
      return;
    }
    combined_str.clear();
  }

  DEBUG("Step 2: Delta encoding")
  // Step 2: Delta encoding
  if (!combined_numbers.empty()) {
    result_data.push_back(vector<uint64_t>());
    auto &delta_encoded = result_data.back();
    delta_encoded.reserve(combined_numbers.size());

    uint64_t base_number = combined_numbers[0];
    delta_encoded.push_back(base_number);

    for (size_t i = 1; i < combined_numbers.size(); ++i) {
      int64_t delta =
          int64_t(combined_numbers[i]) - int64_t(combined_numbers[i - 1]);
      uint64_t encoded_delta =
          (delta >= 0) ? (uint64_t(delta) << 1) : (uint64_t(-delta) << 1) | 1;
      delta_encoded.push_back(encoded_delta);
    }
  }

  // Step 3: Encode and store
  string output_name = "delta_";
  for (size_t i = 0; i < length.size(); ++i) {
    output_name += to_string(length[i]);
    output_name += "_";
  }
  output_name.pop_back();
  DEBUG("SubTokenCompressor::encode_and_store_trans_matrix_lsb in")
  SubTokenCompressor::encode_and_store_trans_matrix_lsb(
      output_dir, "_" + to_string(dict_id) + "_" + output_name, result_data,
      result_data[0].size());
  DEBUG("SubTokenCompressor::encode_and_store_trans_matrix_lsb out")
}

void LogParser::process_single_matrix_original(uint64_t dict_id,
                                               MatrixNdarray &matirx_ndarray) {

  auto &arr = matirx_ndarray.arr;
  auto &arr_idx = matirx_ndarray.arr_idx;
  auto &length = matirx_ndarray.position_length;
  auto &is_numeric_vec = matirx_ndarray.is_numeric_vec;
  auto &has_leading_zero_or_big_number =
      matirx_ndarray.has_leading_zero_or_big_number_vec;
  size_t col_len = arr_idx.get_col_count(), row_len = arr_idx.get_row_count();

  vector<vector<uint64_t>> result_data;
  DEBUG("for each column ... col_len=%lu, row_len=%lu", col_len, row_len)
  for (size_t col_idx = 0; col_idx < col_len; ++col_idx) {
    DEBUG("loop - col_idx=%lu", col_idx)
    int column_length = length[col_idx];
    bool column_is_numeric = is_numeric_vec[col_idx];
    bool column_has_leading_zero_or_big_number =
        has_leading_zero_or_big_number[col_idx];

    auto &column_data = result_data.emplace_back();
    column_data.reserve(row_len);

    if (column_is_numeric &&
        (column_length > 0 || !column_has_leading_zero_or_big_number)) {
      for (size_t row_idx = 0; row_idx < row_len; ++row_idx) {
        auto &ele = arr[arr_idx.get(row_idx, col_idx)];
        string value = ele.substr(0, ele.find('('));

        uint64_t parsed_value;
        if (try_stoull(value, parsed_value))
          column_data.push_back(parsed_value * 2);
        else
          column_data.push_back(0);
      }
    } else {
      for (size_t row_idx = 0; row_idx < row_len; ++row_idx) {
        auto &ele = arr[arr_idx.get(row_idx, col_idx)];
        string value = ele.substr(0, ele.find('('));

        if (is_numeric(value) && value.length() <= 15 &&
            (value.length() == 1 || value[0] != '0')) {
          uint64_t parsed_value;
          if (try_stoull(value, parsed_value))
            column_data.push_back(parsed_value * 2);
          else
            column_data.push_back(0);
        } else {
          uint64_t token_id;
          token_manager.get_or_register_token_no_split(value, 2, nullptr,
                                                       &token_id);
          column_data.push_back(token_id * 2 + 1);
        }
      }
    }
  }

  if (result_data.empty())
    return;
  DEBUG("Store Result")
  // 存储结果
  string output_name;
  for (size_t i = 0; i < length.size(); ++i) {
    output_name += to_string(length[i]);
    output_name += "_";
  }
  output_name.pop_back();

  SubTokenCompressor::encode_and_store_trans_matrix_lsb(
      output_dir, "_" + to_string(dict_id) + "_" + output_name, result_data,
      result_data[0].size());
}

void LogParser::export_chunk_subtoken_dictionary() {
  auto &subtoken_dict = token_manager.id_to_subtoken;
  if (subtoken_dict.empty())
    return;

  string dictionary_path = output_dir + "/token.txt";

  // 1. 收集并排序
  auto sorted_entries = vector<pair<uint64_t, string>>(subtoken_dict.begin(),
                                                       subtoken_dict.end());
  // 按id升序
  sort(sorted_entries.begin(), sorted_entries.end(),
       [](const pair<uint64_t, string> &a, const pair<uint64_t, string> &b) {
         return a.first < b.first;
       });
  ofstream file(dictionary_path);
  if (!file.is_open()) {
    handle_error(
        format("Failed to create output file: %s", dictionary_path.c_str()));
  }
  // 2. 逐行写入纯token
  for (auto &[_, token] : sorted_entries) {
    file << token << "\n";
  }
  file.flush();
  file.close();
  cout << "Successfully exported " << sorted_entries.size() << " tokens to "
       << dictionary_path << endl;
}

void LogParser::export_unmapped_templates_with_dict_id_for_chunk() {
  string output_path = output_dir + "/template.txt";

  ofstream file(output_path);
  if (!file.is_open()) {
    handle_error(
        format("Failed to create output file: %s", output_path.c_str()));
  }

  auto entries =
      vector<pair<uint32_t, string>>(unmapped_templates_with_dict_id.begin(),
                                     unmapped_templates_with_dict_id.end());

  sort(entries.begin(), entries.end(),
       [](const pair<uint32_t, string> &a, const pair<uint32_t, string> &b) {
         return a.first < b.first;
       });

  for (auto &[i, template_str] : entries) {
    DEBUG("i=%u, template_str=%s", i, template_str.c_str())
    file << template_str << "\n";
  }
  file.flush();
  file.close();
}