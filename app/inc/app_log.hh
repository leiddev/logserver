// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 */
#pragma once
#ifndef _APP_LOG_HH_
#define _APP_LOG_HH_

#include <cstddef>
#include <cstdio>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace applog {

enum Level { kTrace, kDebug, kInfo, kWarn, kError, kCritical };

std::string vformat(const char *format, std::size_t arg_count, const std::string *args);

inline std::string toString(const std::string &value) {
  return value;
}

inline std::string toString(const char *value) {
  return value ? std::string(value) : std::string();
}

inline std::string toString(bool value) {
  return value ? "true" : "false";
}

template <typename T>
std::string toString(const T &value) {
  std::ostringstream stream;
  stream << value;
  return stream.str();
}

// Only the "{}" placeholder is supported.
template <typename... Args>
std::string format(const char *fmt, const Args &...args) {
  std::vector<std::string> values;
  int unpack[] = {0, (values.push_back(toString(args)), 0)...};
  (void)unpack;
  return vformat(fmt, values.size(), values.empty() ? nullptr : values.data());
}

// Process wide logger.
// Internal messages go to stdout and to the log file (when enabled).
// Raw messages, i.e. the logs received over UDP, go to the log file only.
class Logger {
 public:
  static Logger &instance();

  void setLevel(Level level);

  // Enable file logging: <dir>/<base_name>.log is the current file, and every
  // rotation renames it to <dir>/<base_name>_YYYYMMDD_HHMMSS_mmm.log.
  // At most max_files files (current file included) are kept.
  // Returns false when max_files is zero or dir is not on a temporary file
  // system (writing flash on embedded devices should be avoided).
  bool initFile(const std::string &dir, const std::string &base_name, std::size_t max_file_size,
                std::size_t max_files);

  template <typename... Args>
  void writef(Level level, const char *fmt, const Args &...args) {
    write(level, format(fmt, args...));
  }

  // Write bytes as they are, without formatting and without adding a newline.
  void writeRaw(const char *data, std::size_t length);

  std::string filePath();

 private:
  Logger() = default;
  ~Logger();
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  void write(Level level, const std::string &message);
  bool openFileLocked();
  void closeFileLocked();
  void writeFileLocked(const char *data, std::size_t length);
  void rotateLocked();
  void cleanupLocked();
  std::string archivePathLocked();

  // The following members are guarded by mutex_.
  mutable std::mutex mutex_;
  Level level_ = kTrace;
  std::string dir_;
  std::string base_name_;
  std::string base_path_;
  std::size_t max_file_size_ = 0;
  std::size_t max_files_ = 0;
  std::size_t current_size_ = 0;
  std::FILE *file_ = nullptr;
  bool open_error_reported_ = false;
};

}  // namespace applog

#endif  // !_APP_LOG_HH_
