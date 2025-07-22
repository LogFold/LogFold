#ifndef LOGMD_LOGPARSER_HPP
#define LOGMD_LOGPARSER_HPP
#include "TokenManager.hpp"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "arg.hpp"
#include "typedef.hpp"
#include "utils/Array2.hpp"
#include "utils/IndexMap.hpp"
#include "utils/pcre2regex.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <utils/IndexManager.hpp>
#include <vector>

class LogParser {
private:
  IndexMap<std::string, PatternContianer> sole_pat_dict;
  uint32_t raw_id_counter = 0;
  uint32_t parsed_id_counter = 0;
  std::string output_dir;
  StrToU32 template_index;
  const Args args;

  // absl::flat_hash_map<uint32_t, Statements> template_to_statement;
  void add_to_exp_rules_dict(const absl::flat_hash_set<VecS> &sole_pat_set,
                             const std::string &value);
  void add_to_matrix_ndarray_dict(const VecS &update_pat_vec,
                                  const absl::flat_hash_set<VecS> &sole_pat_set,
                                  const std::vector<size_t> &keep_column_idx,
                                  const std::string &final_pat_key,
                                  Array2<std::string> &arr);
  void add_to_new_patterns(const VecS &update_pat_vec,
                           const VecS &all_unique_values, VecS &new_patterns,
                           VecS &new_pat_keys);

  bool should_use_delta_optimization(MatrixNdarray &matirx_ndarray);
  bool is_suitable_for_delta_encoding(const std::vector<uint64_t> &numbers);

public:
  std::vector<std::pair<VecS, uint32_t>> runtime_space;
  // std::vector<uint32_t> raw_to_parsed_map;
  U32ToStr parsed_log_map;
  StrToStr exp_rules_dict;
  DynamicSubTokenManager token_manager;
  U32ToStr unmapped_templates_with_dict_id;
  Pcre2Regex MAIN_TOKEN_RE, DYNAMIC_TOKEN_RE;
  std::vector<Pcre2Regex> CLASSIFY_PATTERNS;
  LogParser(const Args &args); // 构造函数创建独立的token管理器
  void update_output_dir(const std::string &output_dir);
  void process_chunk(const VecS &chunk, const IndexManager &index_manager,
                     size_t chunk_idx);
  void parse_one(const std::string &log);
  size_t classify_and_process_token(std::string token, VecS &template_parts,
                                    VecS &total_dynamic_vars);
  // bool parse_template(const std::string &log, uint32_t &parsed_id,
  //                     VecS &total_dynamic_vars);
  bool parse_template_and_process_dynamic_vars(const std::string &log,
                                               std::string &templ,
                                               VecS &total_dynamic_vars);

  bool process_dynamic_token(const std::string &token);
  uint32_t manage_template(const std::string &templ);
  void process_patterns_exp();
  void generate_rules(const VecS &itemset, std::string &rules);

  void
  find_representative_values(const std::map<std::string, uint32_t> &counts_map,
                             const uint32_t rows, VecS &values,
                             uint32_t &rep_sum);
  double calculate_entropy(const std::map<std::string, uint32_t> &counts_map,
                           const uint32_t rows);
  void classify_dataset(const Array2<std::string> &arr,
                        const ColumnIndex &column_index,
                        const VecS &representatives);

  void process_matrix_ndarray_dict();
  void process_single_matrix(uint64_t dict_id, MatrixNdarray &matirx_ndarray);
  void process_matrix_with_delta_optimization(uint64_t dict_id,
                                              MatrixNdarray &matirx_ndarray);
  void process_single_matrix_original(uint64_t dict_id,
                                      MatrixNdarray &matirx_ndarray);
  void export_chunk_subtoken_dictionary();
  void export_unmapped_templates_with_dict_id_for_chunk();
};
#endif // LOGMD_LOGPARSER_HPP