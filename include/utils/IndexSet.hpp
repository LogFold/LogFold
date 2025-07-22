#ifndef LOGMD_INDEX_SET_HPP
#define LOGMD_INDEX_SET_HPP

#include "absl/container/flat_hash_set.h"
#include <vector>
template <class T> class IndexSet {
private:
  absl::flat_hash_set<T> index_set;
  std::vector<T> data;

public:
  IndexSet() = default;
  void insert(const T &value) {
    if (index_set.count(value) == 0) {
      index_set.insert(value);
      data.emplace_back(value);
    }
  }

  void emplace(T &&value) {
    if (index_set.count(value) == 0) {
      index_set.insert(value);
      data.emplace_back(std::move(value));
    }
  }

  bool contains(const T &value) const { return index_set.contains(value); }
  std::vector<T> &to_vector() { return data; }
};

#endif // LOGMD_INDEX_SET_HPP