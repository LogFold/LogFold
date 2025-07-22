#ifndef LOGMD_TOKENMANAGER_HPP
#define LOGMD_TOKENMANAGER_HPP

#include "utils/IndexMap.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <typedef.hpp>
#include <utils/IndexSet.hpp>
#include <vector>

class DynamicSubTokenManager {
public:
  VecVecU64 num_subtoken_vec;
  StrToU64 subtoken_to_id;
  U64ToStr id_to_subtoken;
  uint64_t string_counter = 0;
  IndexSet<std::string> simple_var_dict;
  // StrToU32 token_frequency;
  IndexMap<uint64_t, MatrixNdarray> matrix_ndarray_dict;

  DynamicSubTokenManager();
  void get_or_register_token_no_split(const std::string &token,
                                      const int8_t init_flag, int8_t *flag,
                                      uint64_t *ret_id);
  uint64_t get_or_register_string(const std::string &token);
  void process_base_dict_for_vec(const std::string &output_dir);
  void process_simple_var_dict();
};

class SubTokenCompressor {
public:
  static bool calc_compression_mode(std::vector<int64_t> &nums);
  static std::vector<int64_t>
  compute_delta_values(const std::vector<uint64_t> &nums);
  static void encode_and_store_base_binary(const std::string &output_dir,
                                           const uint32_t key1,
                                           const uint32_t key2,
                                           const std::vector<uint64_t> &vec);
  static void encode_and_store_trans_matrix_lsb(
      const std::string &output_dir, const std::string &output_name,
      const std::vector<std::vector<uint64_t>> &trans_num_matrix,
      size_t expected_row_length);
  static void
  encode_and_store_template_id(const std::string &output_dir,
                               const std::string &output_name,
                               const std::vector<uint32_t> &tmpl_ids);
  static void write_unsigned_leb128(std::ofstream &writer, char *buff,
                                    size_t &offset, uint64_t value);
  static void write_signed_leb128s(std::ofstream &writer,
                                   const std::vector<int64_t> &nums);
  static void compress_chunk(const std::vector<uint8_t> &buffer,
                             const std::string &output_path);
  static void batch_encode_dynamic(const std::vector<uint64_t> &dynamic,
                                   std::vector<uint8_t> &buffer);
};
#endif // LOGMD_TOKENMANAGER_HPP