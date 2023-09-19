/*
 * leid 2022.8
 */
#include "bl/log.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/udp_sink.h"
#include <iostream>

namespace bl {

void Log::init(std::string &logger_name, std::string &level) {
  try {
    auto console_sink_ptr = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    logger_ptr_ = std::make_shared<spdlog::logger>(logger_name, console_sink_ptr);
    spdlog::set_default_logger(logger_ptr_);
    setLogLevel(level);
  } catch (const spdlog::spdlog_ex &ex) {
    std::cout << "Log initialization failed: " << ex.what() << std::endl;
    setLogLevel(level);
  }
}

void Log::init(std::string &log_file_path, std::string &logger_name, std::string &level, size_t max_file_size,
               size_t max_files) {
  try {
    auto console_sink_ptr = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto rotating_file_sink_ptr =
        std::make_shared<spdlog::sinks::rotating_file_sink_mt>(log_file_path, max_file_size, max_files);

    spdlog::sinks_init_list sink_list = {console_sink_ptr, rotating_file_sink_ptr};

    logger_ptr_ = std::make_shared<spdlog::logger>(logger_name, sink_list);
    spdlog::set_default_logger(logger_ptr_);
    setLogLevel(level);
  } catch (const spdlog::spdlog_ex &ex) {
    std::cout << "Log initialization failed: " << ex.what() << std::endl;
    setLogLevel(level);
  }
}

void Log::init(std::string &server_host, uint16_t server_port, std::string &logger_name, std::string &level) {
  try {
    auto console_sink_ptr = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    
    spdlog::sinks::udp_sink_config udp_cfg(server_host, server_port);
    auto udp_sink_ptr = std::make_shared<spdlog::sinks::udp_sink_mt>(udp_cfg);

    spdlog::sinks_init_list sink_list = {console_sink_ptr, udp_sink_ptr};

    logger_ptr_ = std::make_shared<spdlog::logger>(logger_name, sink_list);
    spdlog::set_default_logger(logger_ptr_);
    setLogLevel(level);
  } catch (const spdlog::spdlog_ex &ex) {
    std::cout << "Log initialization failed: " << ex.what() << std::endl;
    setLogLevel(level);
  }
}

void Log::setLogLevel(const std::string &level) {
  char L = toupper(level[0]);
  if (L == 'T') {  // trace
    spdlog::set_level(spdlog::level::trace);
    spdlog::flush_on(spdlog::level::trace);
  } else if (L == 'D') {  // debug
    spdlog::set_level(spdlog::level::debug);
    spdlog::flush_on(spdlog::level::debug);
  } else if (L == 'I') {  // info
    spdlog::set_level(spdlog::level::info);
    spdlog::flush_on(spdlog::level::info);
  } else if (L == 'W') {  // warn
    spdlog::set_level(spdlog::level::warn);
    spdlog::flush_on(spdlog::level::warn);
  } else if (L == 'E') {  // error
    spdlog::set_level(spdlog::level::err);
    spdlog::flush_on(spdlog::level::err);
  } else if (L == 'C') {  // critical
    spdlog::set_level(spdlog::level::critical);
    spdlog::flush_on(spdlog::level::critical);
  } else {
    std::cout << "Log level set error " + level << std::endl;
  }
}
}  // namespace bl