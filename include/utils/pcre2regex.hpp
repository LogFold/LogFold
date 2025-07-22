#ifndef LOGMD_PCRE2REGEX_HPP
#define LOGMD_PCRE2REGEX_HPP
#define PCRE2_CODE_UNIT_WIDTH 8
#include <cstdarg>
#include <cstddef>
#include <memory>
#include <pcre2.h>
#include <string>
#include <utility>
#include <utils/util.hpp>
#include <vector>
inline void handle_pcre2_error(const char *error_msg, int error_code, ...) {
  va_list args;
  va_start(args, error_code);
  char buffer[256];
  pcre2_get_error_message_8(error_code, (PCRE2_UCHAR8 *)buffer, sizeof(buffer));
  std::ostringstream oss;
  const char *fmt = error_msg;
  while (*fmt != '\0') {
    if (*fmt == '%') {
      ++fmt;
      if (*fmt == 's') {
        oss << error_code;
        ++fmt;
        break;
      } else {
        oss << '%' << *fmt;
      }
    } else {
      oss << *fmt;
    }
    ++fmt;
  }
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
  handle_error(oss.str());
}

class Pcre2RegexIterator {
  PCRE2_SPTR c_str;
  size_t length;
  size_t start_offset;
  pcre2_code *code;
  pcre2_match_data *match_data;
  pcre2_match_context *match_context;
  bool is_end;
  PCRE2_SIZE *ovector;
  int rc;

public:
  // Pcre2RegexIterator() = default;
  // 禁用拷贝
  Pcre2RegexIterator(const Pcre2RegexIterator &) = delete;
  Pcre2RegexIterator &operator=(const Pcre2RegexIterator &) = delete;
  Pcre2RegexIterator(const std::string &subject, pcre2_code *code,
                     pcre2_match_data *match_data,
                     pcre2_match_context *match_context)
      : c_str(reinterpret_cast<PCRE2_SPTR>(subject.c_str())),
        length(subject.length()), start_offset(0), code(code),
        match_data(match_data), match_context(match_context), is_end(false) {
    // if (start_offset >= length)
    //   is_end = true;
    next();
  }

  ~Pcre2RegexIterator() { pcre2_match_data_free(match_data); }

  void next() {
    if (is_end || start_offset >= length) {
      is_end = true;
      return;
    }
    rc = pcre2_match(code, c_str, length, start_offset, 0, match_data,
                     match_context);
    if (rc < 0) {
      if (rc == PCRE2_ERROR_NOMATCH)
        is_end = true;
      else
        handle_pcre2_error("IN iterator.next - Matching error: %s", rc);

    } else {
      ovector = pcre2_get_ovector_pointer(match_data);
      start_offset = ovector[1];
      if (ovector[0] == ovector[1])
        ++start_offset;
    }
  }

  bool has_cap(int i = 0) const {
    if (i > rc || i < 0)
      handle_error("IN has_cap - Invalid capture index: %d", i);
    return ovector[2 * i] < ovector[2 * i + 1];
  }
  std::pair<size_t, size_t> cap(int i = 0) {
    if (i > rc || i < 0)
      handle_error("IN cap - Invalid capture index: %d", i);
    return {ovector[2 * i], ovector[2 * i + 1]};
  }

  bool end() const { return is_end; }
};

class Pcre2Regex {
public:
  // 构造函数：编译正则表达式并启用 JIT
  explicit Pcre2Regex(const std::string &pattern, uint32_t options = 0) {
    int errorcode;
    PCRE2_SIZE erroroffset;

    // 编译正则表达式
    code_ = pcre2_compile_8(
        reinterpret_cast<PCRE2_SPTR>(pattern.c_str()), PCRE2_ZERO_TERMINATED,
        options | PCRE2_UTF | PCRE2_UCP, &errorcode, &erroroffset, nullptr);

    if (!code_)
      handle_pcre2_error("Regex compilation failed: %s at offset %d", errorcode,
                         erroroffset);

    // 启用 JIT 编译
    errorcode = pcre2_jit_compile_8(code_, PCRE2_JIT_COMPLETE);
    if (errorcode) {
      pcre2_code_free(code_);
      handle_pcre2_error("JIT compilation failed with error_code: %s",
                         errorcode);
    }

    // 创建全局缓存
    jit_stack_ = pcre2_jit_stack_create_8(32 * 1024, 512 * 1024, nullptr);
    if (!jit_stack_) {
      pcre2_code_free(code_);
      handle_error("JIT stack creation failed");
    }

    // 创建匹配上下文并分配 JIT 栈
    match_context_ = pcre2_match_context_create_8(nullptr);
    if (!match_context_) {
      pcre2_jit_stack_free(jit_stack_);
      pcre2_code_free(code_);
      handle_error("Match context creation failed");
    }

    pcre2_jit_stack_assign(match_context_, nullptr, jit_stack_);
  }

  explicit Pcre2Regex() = default;

  // 析构函数：释放所有资源
  ~Pcre2Regex() {
    if (match_context_)
      pcre2_match_context_free(match_context_);
    if (jit_stack_)
      pcre2_jit_stack_free(jit_stack_);
    if (code_)
      pcre2_code_free(code_);
  }

  // 禁用拷贝
  Pcre2Regex(const Pcre2Regex &) = delete;
  Pcre2Regex &operator=(const Pcre2Regex &) = delete;

  // 移动构造函数
  Pcre2Regex(Pcre2Regex &&other) noexcept
      : code_(other.code_), jit_stack_(other.jit_stack_),
        match_context_(other.match_context_) {
    other.code_ = nullptr;
    other.jit_stack_ = nullptr;
    other.match_context_ = nullptr;
  }

  // 移动赋值运算符
  Pcre2Regex &operator=(Pcre2Regex &&other) noexcept {
    if (this != &other) {
      this->~Pcre2Regex();
      code_ = other.code_;
      jit_stack_ = other.jit_stack_;
      match_context_ = other.match_context_;
      other.code_ = nullptr;
      other.jit_stack_ = nullptr;
      other.match_context_ = nullptr;
    }
    return *this;
  }

  // 匹配操作
  bool match(const std::string &subject, uint32_t options = 0) const {
    pcre2_match_data *match_data =
        pcre2_match_data_create_from_pattern(code_, nullptr);
    if (!match_data)
      handle_error("Match data creation failed");

    int rc = pcre2_match(code_, reinterpret_cast<PCRE2_SPTR>(subject.c_str()),
                         subject.length(),
                         0, // start offset
                         options, match_data, match_context_);

    pcre2_match_data_free(match_data);

    if (rc < 0) {
      if (rc == PCRE2_ERROR_NOMATCH)
        return false;
      handle_pcre2_error("IN match - Matching error: %s", rc);
    }
    return true;
  }

  // 获取所有匹配结果
  std::vector<std::string> get_matches(const std::string &subject,
                                       uint32_t options = 0) const {
    std::vector<std::string> matches;
    pcre2_match_data *match_data =
        pcre2_match_data_create_from_pattern(code_, nullptr);
    if (!match_data)
      handle_error("Match data creation failed");

    int rc =
        pcre2_match(code_, reinterpret_cast<PCRE2_SPTR>(subject.c_str()),
                    subject.length(), 0, options, match_data, match_context_);

    if (rc < 0) {
      pcre2_match_data_free(match_data);
      if (rc == PCRE2_ERROR_NOMATCH)
        return matches;
      handle_pcre2_error("IN get_matches - Matching error: %s", rc);
    }

    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(match_data);
    for (int i = 0; i < rc; ++i) {
      PCRE2_SIZE start = ovector[2 * i];
      PCRE2_SIZE end = ovector[2 * i + 1];
      matches.emplace_back(subject.substr(start, end - start));
    }

    pcre2_match_data_free(match_data);
    return matches;
  }

  Pcre2RegexIterator get_iter(const std::string &subject) {
    auto match_data = pcre2_match_data_create_from_pattern(code_, nullptr);
    return Pcre2RegexIterator(subject, code_, match_data, match_context_);
  }

private:
  pcre2_code *code_ = nullptr;
  pcre2_jit_stack *jit_stack_ = nullptr;
  pcre2_match_context *match_context_ = nullptr;
};

#endif // LOGMD_PCRE2REGEX_HPP