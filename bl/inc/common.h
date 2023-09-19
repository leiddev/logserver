#ifndef _BL_COMMON_H_
#define _BL_COMMON_H_

#include "bl/log.h"

#define LOG_TRACE(...) spdlog::trace("[bl] " __VA_ARGS__)
#define LOG_DEBUG(...) spdlog::debug("[bl] " __VA_ARGS__)
#define LOG_INFO(...) spdlog::info("[bl] " __VA_ARGS__)
#define LOG_WARN(...) spdlog::warn("[bl] " __VA_ARGS__)
#define LOG_ERR(...) spdlog::error("[bl] " __VA_ARGS__)
#define LOG_CRITICAL(...) spdlog::critical("[bl] " __VA_ARGS__)

#define CHECK_PTR(p)                                                    \
  do {                                                                  \
    if (!p) {                                                           \
      LOG_ERR("[{}] [{}] " #p " null pointer", __FUNCTION__, __LINE__); \
      return -1;                                                        \
    }                                                                   \
  } while (0)

#define CHECK_RANGE(p, min, max)                                                                    \
  do {                                                                                              \
    if (p < min || p > max) {                                                                       \
      LOG_ERR("[{}] [{}] " #p " :[{}] out of range [{}, {}]", __FUNCTION__, __LINE__, p, min, max); \
      return -1;                                                                                    \
    }                                                                                               \
  } while (0)

#define CHECK_ERR(s)                                                        \
  do {                                                                      \
    if (s) {                                                                \
      LOG_ERR("[{}]--- [{}] --- failed --- {}", __FUNCTION__, __LINE__, s); \
      return s;                                                             \
    }                                                                       \
  } while (0)

#endif /* _bl_COMMON_H_ */
