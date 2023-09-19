#ifndef _APP_COMMON_H_
#define _APP_COMMON_H_

#include "bl/log.h"

#define APP_VERSION_STR      "V0.1"

#define LOG_TRACE(...) spdlog::trace("[APP] " __VA_ARGS__)
#define LOG_DEBUG(...) spdlog::debug("[APP] " __VA_ARGS__)
#define LOG_INFO(...) spdlog::info("[APP] " __VA_ARGS__)
#define LOG_WARN(...) spdlog::warn("[APP] " __VA_ARGS__)
#define LOG_ERR(...) spdlog::error("[APP] " __VA_ARGS__)
#define LOG_CRITICAL(...) spdlog::critical("[APP] " __VA_ARGS__)


#define CHECK_PTR(p)                                                       \
  do {                                                                     \
    if (!p) {                                                              \
      LOG_ERR("[{}] [{}] " #p " null pointer", __FUNCTION__, __LINE__); \
      return -1;                                                           \
    }                                                                      \
  } while (0)

#define CHECK_RANGE(p, min, max)                                                                      \
  do {                                                                                                \
    if (p < min || p > max) {                                                                         \
      LOG_ERR("[{}] [{}] " #p " :[{}] out of range [{}, {}]", __FUNCTION__, __LINE__, p, min, max); \
      return -1;                                                                                      \
    }                                                                                                 \
  } while (0)

#define CHECK_ERR(s)                                                            \
  do {                                                                          \
    if (s) {                                                                    \
      LOG_ERR("[{}]--- [{}] --- failed --- {}", __FUNCTION__, __LINE__, s); \
      return s;                                                                 \
    }                                                                           \
  } while (0)

#endif /* _APP_COMMON_H_ */