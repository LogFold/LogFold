#ifndef LOGMD_TYPEDEF_HPP
#define LOGMD_TYPEDEF_HPP

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "constant.hpp"
#include "utils/Array2.hpp"
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

typedef std::vector<std::string> VecS;
typedef std::vector<std::vector<uint64_t>> VecVecU64;
// typedef absl::flat_hash_map<uint32_t, std::vector<uint64_t>> U32ToVecU64;
typedef absl::flat_hash_map<std::string, uint64_t> StrToU64;
typedef absl::flat_hash_map<uint64_t, std::string> U64ToStr;
typedef absl::flat_hash_map<std::string, uint32_t> StrToU32;
typedef absl::flat_hash_map<uint32_t, std::string> U32ToStr;
// typedef absl::flat_hash_map<uint32_t, uint32_t> U32ToU32;
typedef absl::flat_hash_map<std::string, std::string> StrToStr;

struct PatternContianer {
  absl::flat_hash_set<VecS> set;
  std::vector<VecS> vec;
};

struct SlotStats {
  size_t column_index;
  size_t unique_values_count;
  VecS representative_values;
  double dominance_ratio;
  double entropy;
};

struct MatrixNdarray {
  Array2<std::string> arr;
  SubArrayIndex arr_idx;
  std::vector<int32_t> position_length;
  std::vector<bool> is_numeric_vec;
  std::vector<bool> has_leading_zero_or_big_number_vec;
  MatrixNdarray() = default;
  MatrixNdarray(const MatrixNdarray &other) {
    arr = other.arr;
    arr_idx = other.arr_idx;
    position_length = other.position_length;
    is_numeric_vec = other.is_numeric_vec;
    has_leading_zero_or_big_number_vec =
        other.has_leading_zero_or_big_number_vec;
  };

  MatrixNdarray(MatrixNdarray &&other) {
    arr = other.arr;
    arr_idx = std::move(other.arr_idx);
    position_length = std::move(other.position_length);
    is_numeric_vec = std::move(other.is_numeric_vec);
    has_leading_zero_or_big_number_vec =
        std::move(other.has_leading_zero_or_big_number_vec);
  }
};

#endif // LOGMD_TYPEDEF_HPP