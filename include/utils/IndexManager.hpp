#ifndef LOGMD_INDEXMANAGER_HPP
#define LOGMD_INDEXMANAGER_HPP

#include <cstddef>
class IndexManager {
private:
  size_t base;
  size_t size;

public:
  IndexManager(size_t base, size_t size) : base(base), size(size) {}
  size_t operator[](const size_t i) const { return base + i; }
  size_t len() const { return size; }
};

// class IndexManager2D {
//     private:

// }

#endif // LOGMD_INDEXMANAGER_HPP