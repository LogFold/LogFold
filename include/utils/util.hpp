#ifndef LOGMD_UTIL_HPP
#define LOGMD_UTIL_HPP

#include "absl/container/flat_hash_set.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>
#include <vector>

bool is_numeric(const std::string &);
std::string format(const std::string format_str, ...);
// 错误处理函数
void handle_error(const std::string &message, int exit_code = 1);
bool try_stoul(const std::string &str, uint32_t &value);
bool try_stoull(const std::string &str, uint64_t &value);
void find_frequent_patterns(
    const absl::flat_hash_set<std::vector<std::string>> &transactions,
    size_t min_support);
std::string number2letter(int number);

#endif // LOGMD_UTIL_HPP