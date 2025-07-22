#include "internal/out.hpp"
#include "typedef.hpp"
#include <LogParser.hpp>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <utils/util.hpp>
using namespace std;

void LogParser::add_to_exp_rules_dict(
    const absl::flat_hash_set<VecS> &sole_pat_set, const string &value) {
  for (const auto &tokens : sole_pat_set) {
    string token_str;
    generate_rules(tokens, token_str);
    exp_rules_dict.emplace(move(token_str), value);
  }
}

void LogParser::add_to_matrix_ndarray_dict(
    const VecS &update_pat_vec, const absl::flat_hash_set<VecS> &sole_pat_set,
    const vector<size_t> &keep_column_idx, const string &final_pat_key,
    Array2<string> &arr) {

  //注册pattern-id
  uint64_t dict_id;
  token_manager.get_or_register_token_no_split(final_pat_key, 1, nullptr,
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
  matirx_ndarray.arr_idx = arr.get_columns(keep_column_idx);

  //遍历missing_matrix_for_final_pat_key的每一列，获得每一列的unique的值的个数
  for (size_t col_idx = 0; col_idx < keep_column_idx.size(); ++col_idx) {
    absl::flat_hash_set<size_t> length_vec;
    bool is_num = true;
    bool has_leading_zero_or_big_number = false;

    auto column_index = arr.get_column(keep_column_idx[col_idx]);
    IndexSet<string> unique_set;
    for (size_t i = 0; i < column_index.size(); ++i)
      unique_set.insert(arr[column_index.get(i)]);
    auto &unique_values = unique_set.to_vector();

    for (const auto &ele : unique_values) {
      string pur_ele = ele.substr(0, ele.find('('));

      if (is_num && !is_numeric(pur_ele))
        is_num = false;

      size_t len = pur_ele.size();
      length_vec.insert(len);

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
}

void LogParser::add_to_new_patterns(const VecS &update_pat_vec,
                                    const VecS &all_unique_values,
                                    VecS &new_patterns, VecS &new_pat_keys) {
  DEBUG("parser.add_to_new_patterns in")
  auto new_update_pat_vec = update_pat_vec;
  new_update_pat_vec.push_back("");
  auto &back_pat = new_update_pat_vec.back();
  size_t back_pos = new_patterns.size();
  new_patterns.resize(new_patterns.size() + all_unique_values.size());
  new_pat_keys.reserve(new_pat_keys.size() + all_unique_values.size());
  for (const auto &ele : all_unique_values) {
    back_pat = ele;
    new_pat_keys.push_back(ele);
    DEBUG("generate_rules in")
    generate_rules(new_update_pat_vec, new_patterns[back_pos++]);
    DEBUG("generate_rules out")
  }
  DEBUG("parser.add_to_new_patterns out")
}

bool LogParser::should_use_delta_optimization(MatrixNdarray &matirx_ndarray) {
  auto &arr = matirx_ndarray.arr;
  auto &arr_idx = matirx_ndarray.arr_idx;
  auto &length = matirx_ndarray.position_length;
  auto &is_numeric_vec = matirx_ndarray.is_numeric_vec;

  bool is_all_fixed_length = all_of(length.begin(), length.end(),
                                    [](int32_t len) { return len != -1; });

  bool is_all_numeric = all_of(is_numeric_vec.begin(), is_numeric_vec.end(),
                               [](bool is_num) { return is_num; });
  size_t row_len = arr_idx.get_row_count(), col_len = arr_idx.get_col_count();

  bool has_multiple_columns = col_len >= 2;
  bool has_sufficient_rows = row_len >= 2;

  if (!(is_all_fixed_length && is_all_numeric && has_multiple_columns &&
        has_sufficient_rows)) {
    return false;
  }

  size_t sample_size = min<size_t>(10, row_len);
  vector<uint64_t> sample_numbers;
  sample_numbers.reserve(sample_size);

  string combined_str;
  for (size_t row_idx = 0; row_idx < sample_size; ++row_idx) {
    // auto row = matrix.row(row_idx);
    for (size_t col_idx = 0; col_idx < col_len; ++col_idx) {
      auto &ele = arr[arr_idx.get(row_idx, col_idx)];
      string clean_value = ele.substr(0, ele.find('('));

      size_t expected_len = length[col_idx];

      // 填充到指定长度
      combined_str += string(expected_len - clean_value.length(), '0');
      combined_str += clean_value;
    }
    uint64_t combined_num;
    if (try_stoull(combined_str, combined_num)) {
      sample_numbers.push_back(combined_num);
    } else {
      cout << "Failed to parse sample number: " << combined_str
           << ", skipping delta optimization" << endl;
      return false;
    }
    combined_str.clear();
  }

  return is_suitable_for_delta_encoding(sample_numbers);
}

bool LogParser::is_suitable_for_delta_encoding(
    const std::vector<uint64_t> &numbers) {
  if (numbers.size() < 2) {
    return false;
  }

  vector<uint64_t> deltas;
  deltas.reserve(numbers.size());
  for (size_t i = 1; i < numbers.size(); ++i) {
    if (numbers[i] >= numbers[i - 1]) {
      deltas.push_back(numbers[i] - numbers[i - 1]);
    } else {
      deltas.push_back(numbers[i - 1] - numbers[i]);
    }
  }

  uint64_t max_original = *max_element(numbers.begin(), numbers.end());
  uint64_t max_delta = *max_element(deltas.begin(), deltas.end());

  double delta_ratio = double(max_delta) / double(max_original);
  bool is_suitable = delta_ratio < 0.1;

  // 可选调试输出
  /*
  std::cout << "Delta suitability check: sample_size=" << numbers.size()
            << ", max_original=" << max_original
            << ", max_delta=" << max_delta
            << ", ratio=" << delta_ratio
            << ", suitable=" << std::boolalpha << is_suitable << std::endl;
  */

  return is_suitable;
}