#ifndef LOGMD_INDEX_MAP_HPP
#define LOGMD_INDEX_MAP_HPP

#include "absl/container/flat_hash_map.h"
#include <cstddef>
#include <utility>
#include <vector>
template <class K, class V> class IndexMap {
private:
  absl::flat_hash_map<K, size_t> index_map;
  std::vector<std::pair<K, V>> data;

public:
  IndexMap() = default;
  void insert(const K &key, const V &value) {
    if (index_map.try_emplace(key, data.size()).second) {
      data.emplace_back(key, value);
    }
  }
  std::vector<std::pair<K, V>> &to_vector() { return data; }

  V &operator[](const K &key) {
    if (index_map.find(key) == index_map.end()) {
      insert(key, V());
    }
    return data[index_map[key]].second;
  }
};

#endif // LOGMD_INDEX_MAP_HPP