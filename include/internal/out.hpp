#ifndef LOGMF_INCLUDE_INTERNAL_OUT_HPP
#define LOGMF_INCLUDE_INTERNAL_OUT_HPP

#define DEBUG(...) void(0);

#ifndef DEBUG
#include <cstdio>
#define DEBUG(...)                                                             \
  do {                                                                         \
    std::printf("DEBUG: ");                                                    \
    std::printf(__VA_ARGS__);                                                  \
    std::printf("\n");                                                         \
  } while (0);
#endif

#endif // LOGMF_INCLUDE_INTERNAL_OUT_HPP