#include "internal/out.hpp"
#include <TokenManager.hpp>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <utils/util.hpp>

DynamicSubTokenManager::DynamicSubTokenManager() {
  num_subtoken_vec.resize(16); // 16位数字集合
}

void DynamicSubTokenManager::get_or_register_token_no_split(
    const std::string &token, const int8_t init_flag, int8_t *flag,
    uint64_t *ret_id) {
  bool is_num = is_numeric(token);
  if (init_flag == -1 && is_num) {
    size_t length = token.length();

    // 超长数字检查 (长度>15且不以0开头)
    if (length > 15 && token[0] != '0') {
      handle_error(format("The num is too big, length: %d, token: %s. Not "
                          "register it. Not collect it.",
                          length, token.c_str()));
    } else {
      // 存储到数字集合
      num_subtoken_vec[length].emplace_back(std::stoull(token));
      return;
    }
  }

  // 数字令牌处理
  if (is_num && (token.length() <= 1 || token[0] != '0') &&
      token.length() <= 15) {
    handle_error(
        format("IN GET_OR_REGISTER_TOKEN_NO_SPLIT: base-number token: %s, "
               "length: %d. Not register it. Not collect it.",
               token.c_str(), token.length()));
  }
  // 前导0数字 (长度>1 且以0开头) || 超大数字 (长度>15) || 非数字
  auto id = get_or_register_string(token);
  // if (flag)
  //   *flag = 0b01;
  if (ret_id)
    *ret_id = id;
}

uint64_t
DynamicSubTokenManager::get_or_register_string(const std::string &token) {
  // 1. 尝试查找已存在的token
  auto it = subtoken_to_id.find(token);
  if (it != subtoken_to_id.end()) {
    return it->second;
  }

  // 2. Token不存在，使用全局统一的ID分配器
  uint64_t id = string_counter++;
  // 尝试插入，如果已存在则返回现有值
  DEBUG("IN GET_OR_REGISTER_STRING - token: %s, id: %lu", token.c_str(), id)
  subtoken_to_id[token] = id;
  id_to_subtoken[id] = token;

  return id;
}

void DynamicSubTokenManager::process_base_dict_for_vec(
    const std::string &output_dir) {
  for (size_t i = 1; i <= 15; i++) {
    SubTokenCompressor::encode_and_store_base_binary(output_dir, i, 0,
                                                     num_subtoken_vec[i]);
  }
}

void DynamicSubTokenManager::process_simple_var_dict() {
  auto &vec = simple_var_dict.to_vector();
  for (const auto &token : vec) {
    // int8_t encode_mode;
    // uint64_t id;
    get_or_register_token_no_split(token, 0, nullptr, nullptr);

    // if (encode_mode == 0b00) {
    //   // leading-zero 类型，不合法
    //   handle_error(format("IN PROCESS_SIMPLE_VAR_DICT: leading-zero token:
    //   %s, "
    //                       "length: %d. Not register it. Not collect it.",
    //                       token.c_str(), token.length()));

    // } else if (encode_mode == 0b01) {
    //   // 普通 token，正常注册 (已存入，无需操作)
    // subtoken_to_id[token] = id;
    // id_to_subtoken[id] = token;
    // } else if (encode_mode == 0b10) {
    //   // base number 类型，不合法
    //   handle_error(format("IN PROCESS_SIMPLE_VAR_DICT: base number token: %s,
    //   "
    //                       "length: %d. Not register it. Not collect it.",
    //                       token.c_str(), token.length()));
    // } else if (encode_mode == 0b11) {
    //   // big number 类型，当前不支持
    //   handle_error(format("IN PROCESS_SIMPLE_VAR_DICT: big number token: %s,
    //   "
    //                       "length: %d. Not register it. Not collect it.",
    //                       token.c_str(), token.length()));
    // 原计划插入 big_num_dict
    // big_num_dict_[token] = id;
    // big_num_reverse_[id] = token;
    // }
  }
}