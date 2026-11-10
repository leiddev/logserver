// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 *
 * Project wide definitions shared by every translation unit: the version
 * string, the LOG_* macros that route messages through applog, and the
 * argument checking helpers.
 */
#ifndef _APP_COMMON_H_
#define _APP_COMMON_H_

#include "app_log.hh"

#define APP_VERSION_STR      "V0.8.5"

// Internal messages carry the [APP] prefix so they can be told apart from the
// logs received over UDP when both end up in the same file.
#define LOG_TRACE(...) applog::Logger::instance().writef(applog::kTrace, __VA_ARGS__)
#define LOG_DEBUG(...) applog::Logger::instance().writef(applog::kDebug, "[APP] " __VA_ARGS__)
#define LOG_INFO(...) applog::Logger::instance().writef(applog::kInfo, "[APP] " __VA_ARGS__)
#define LOG_WARN(...) applog::Logger::instance().writef(applog::kWarn, "[APP] " __VA_ARGS__)
#define LOG_ERR(...) applog::Logger::instance().writef(applog::kError, "[APP] " __VA_ARGS__)
#define LOG_CRITICAL(...) applog::Logger::instance().writef(applog::kCritical, "[APP] " __VA_ARGS__)

// Log the failed check and return -1 from the enclosing function.
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