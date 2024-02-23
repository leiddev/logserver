/*
 * leid 2022.8
 */
#pragma once

#include "spdlog/spdlog.h"

namespace bl {

class Log {
 private:
  Log() = default;

 private:
  std::shared_ptr<spdlog::logger> logger_ptr_ = nullptr;
  void setLogLevel(const std::string &level);

 public:
  static Log &getInstance() {
    static Log instance;
    return instance;
  }

  void init(std::string &logger_name, std::string &level);

  void init(std::string &log_file_path, std::string &logger_name, std::string &level, size_t max_file_size,
            size_t max_files);

  void init(std::string &server_host, uint16_t server_port, std::string &logger_name, std::string &level);

  std::shared_ptr<spdlog::logger> logger() {
    if (logger_ptr_)
      return logger_ptr_;
    else
      return spdlog::default_logger();
  }

  void setPattern(std::string pattern);
};  // LOG class

}  // namespace bl
