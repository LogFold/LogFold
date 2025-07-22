#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <utils/util.hpp>

bool is_numeric(const std::string &str) {
  return !str.empty() && std::all_of(str.begin(), str.end(), ::isdigit);
}

// 如果需要高性能，请勿使用这个api
std::string format(const std::string format_str, ...) {
  va_list args;
  va_start(args, format_str);

  std::ostringstream oss;
  const char *fmt = format_str.c_str();

  while (*fmt != '\0') {
    if (*fmt == '%') {
      ++fmt;
      if (*fmt == 's') {
        const char *val = va_arg(args, const char *);
        oss << val;
      } else if (*fmt == 'd') {
        long long val = va_arg(args, long long);
        oss << val;
      } else if (*fmt == 'f') {
        double val = va_arg(args, double);
        oss << val;
      } else {
        oss << '%' << *fmt;
      }
    } else {
      oss << *fmt;
    }
    ++fmt;
  }

  va_end(args);
  return oss.str();
}
void handle_error(const std::string &message, int exit_code) {
  std::cerr << "[ERROR]: " << message << std::endl;
  std::exit(exit_code);
}

bool try_stoull(const std::string &str, uint64_t &value) {
  if (str.empty())
    return false;
  value = 0;
  for (char c : str) {
    if (c < '0' || c > '9')
      return false;
    if (value > (UINT64_MAX / 10))
      return false; // 溢出检查
    value = value * 10 + (c - '0');
  }
  return true;
}
bool try_stoul(const std::string &str, uint32_t &value) {
  if (str.empty())
    return false;
  value = 0;
  for (char c : str) {
    if (c < '0' || c > '9')
      return false;
    if (value > (UINT32_MAX / 10))
      return false; // 溢出检查
    value = value * 10 + (c - '0');
  }
  return true;
}

std::string number2letter(int number) {
  if (number <= 25) {
    return std::string(1, 'a' + number);
  } else {
    std::string str;
    str.reserve(number / 26 + 1);
    while (number > 25) {
      int remainder = (number - 26) % 26;
      str.push_back('a' + remainder);
      number = (number - 26) / 26;
    }
    str.push_back('a' + number);
    std::reverse(str.begin(), str.end());
    return str;
  }
}

void find_frequent_patterns(
    const absl::flat_hash_set<std::vector<std::string>> &transactions,
    size_t min_support) {}