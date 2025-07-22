#ifndef LOGMD_ARRAY2_HPP
#define LOGMD_ARRAY2_HPP

#include "absl/container/flat_hash_set.h"
#include <cstddef>
#include <vector>

struct Array2Index {
  size_t row_idx, col_idx;
  Array2Index(size_t r, size_t c) : row_idx(r), col_idx(c) {}
};

class ColumnIndex {
private:
  size_t col_idx;
  size_t _size;

public:
  ColumnIndex(size_t col_idx, size_t size) : col_idx(col_idx), _size(size) {}
  Array2Index get(const size_t i) const { return Array2Index{i, col_idx}; }

  size_t size() const { return _size; }
};

class SubArrayIndex {
private:
  std::vector<size_t> column_index;
  std::vector<size_t> row_index;
  size_t row, col;

public:
  SubArrayIndex() = default;
  SubArrayIndex(const std::vector<size_t> &col_idx, size_t row, size_t col)
      : column_index(col_idx), row(row), col(col) {}

  SubArrayIndex(const SubArrayIndex &other) : row(other.row), col(other.col) {
    column_index = other.column_index;
    row_index = other.row_index;
  };
  SubArrayIndex(SubArrayIndex &&other) : row(other.row), col(other.col) {
    column_index.swap(other.column_index);
    row_index.swap(other.row_index);
  }

  SubArrayIndex &operator=(SubArrayIndex &&other) {
    if (this != &other) {
      row = other.row;
      col = other.col;
      column_index.swap(other.column_index);
      row_index.swap(other.row_index);
    }
    return *this;
  }

  SubArrayIndex &operator=(const SubArrayIndex &other) {
    column_index = other.column_index;
    row_index = other.row_index;
    row = other.row;
    col = other.col;
    return *this;
  }

  Array2Index get(size_t i, size_t j) {
    return row_index.empty() ? Array2Index{i, column_index[j]}
                             : Array2Index{row_index[i], column_index[j]};
  }

  void set_row_index(const std::vector<size_t> &row_idx) {
    row_index = row_idx;
    row = row_index.size();
  }

  void set_row_index_by_swap(std::vector<size_t> &row_idx) {
    row_index.swap(row_idx);
    row = row_index.size();
  }

  void set_col_index(const std::vector<size_t> &col_idx) {
    column_index = col_idx;
    col = column_index.size();
  }

  void set_col_index_by_swap(std::vector<size_t> &col_idx) {
    column_index.swap(col_idx);
    col = column_index.size();
  }

  std::vector<size_t>
  get_columns_to_keep(const absl::flat_hash_set<size_t> &col_to_remove) {

    std::vector<size_t> new_col_idx;
    new_col_idx.reserve(column_index.size() - col_to_remove.size());
    for (size_t i = 0; i < column_index.size(); i++) {
      if (!col_to_remove.contains(i))
        new_col_idx.push_back(column_index[i]);
    }
    return new_col_idx;
  }

  size_t get_row_count() const { return row; }
  size_t get_col_count() const { return col; }
};

class RowIndex {
private:
  size_t row_idx;
  size_t _size;

public:
  RowIndex(size_t row_idx, size_t size) : row_idx(row_idx), _size(size) {}
  Array2Index get(const size_t i) const { return Array2Index{row_idx, i}; }
  size_t size() const { return _size; }
};

template <typename T> class Array2 {
private:
  std::vector<std::vector<T>> *data;
  size_t row, col;

public:
  Array2() = default;
  Array2(std::vector<std::vector<T>> *arr) {
    data = arr;
    row = data->size();
    col = data->front().size();
  }

  size_t get_row_count() const { return row; }
  size_t get_col_count() const { return col; }

  ColumnIndex get_column(size_t col_idx) const {
    return ColumnIndex(col_idx, row);
  }

  SubArrayIndex get_columns(const std::vector<size_t> &col_idx) const {
    return SubArrayIndex(col_idx, row, col_idx.size());
  }
  RowIndex get_row(size_t row_idx) const { return RowIndex(row_idx, col); }

  T &operator[](const Array2Index &idx) {
    return (*data)[idx.row_idx][idx.col_idx];
  }
};

#endif // LOGMD_ARRAY2_HPP