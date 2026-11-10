// SPDX-License-Identifier: MIT
/*
 * Leid 2023.9
 *
 * Program entry point: parses the command line, sets up logging, starts the
 * UDP and TCP servers plus the chosen web front end (SSE or WebSocket), and
 * ties every received datagram to all of the outputs.
 */

// Web Server Configuration:
//   0 - Disable Web Server
//   1 - Use WebSocket Server
//   2 - Use SSE Server
// Set by the WEBSERVER_TYPE CMake option; SSE is used when building this file
// directly without CMake.
#ifndef ENABLE_WEB_SERVER
#define ENABLE_WEB_SERVER 2
#endif

#include <cmdline.h>
#include "tcp_server.hh"
#include "udp_server.hh"
#if ENABLE_WEB_SERVER == 1
  #include "websocket_server.hh"
#elif ENABLE_WEB_SERVER == 2
  #include "sse_server.hh"
#endif
#include "common.h"

#include <thread>
#include <string>

static bool disp_msg = true;
static bool log_msg = false;

int main(int argc, char *argv[]) {
  const std::string app_name{"LogServer"};

  cmdline::parser cmdl;
  cmdl.add<int>("udp_port", 'u', "UDP port number", false, 11000, cmdline::range(1, 65535));
  cmdl.add<int>("tcp_port", 't', "TCP port number", false, 11000, cmdline::range(1, 65535));
  cmdl.add<int>("max_files", 's', "Maximum number of files", false, 100, cmdline::range(0, 1000));
  cmdl.add<std::string>("log_suffix", 'x', "Log file suffix, txt or log", false, "txt",
                        cmdline::oneof<std::string>("txt", "log"));
  cmdl.add("disp_msg", '\0', "display receive message");
#if ENABLE_WEB_SERVER != 0
  cmdl.add<int>("web_port", 'w', "Web port number", false, 11001, cmdline::range(1, 65535));
#endif
  cmdl.parse_check(argc, argv);

  LOG_CRITICAL("=========={} Start==========\n", app_name);
  LOG_CRITICAL("{} App Version: {}\n", app_name, APP_VERSION_STR);

#ifdef _WIN32
#define CODE_PAGE_GBK (936)
#define CODE_PAGE_UTF8 (65001)
  if (IsValidCodePage(CODE_PAGE_UTF8)) {
    SetConsoleCP(CODE_PAGE_UTF8);
    SetConsoleOutputCP(CODE_PAGE_UTF8);
    std::string console_title = app_name + " " + std::string(APP_VERSION_STR);
    SetConsoleTitle(console_title.c_str());
  }
#endif

  std::string log_file_name = "logserver";
  std::string log_file_suffix = "." + cmdl.get<std::string>("log_suffix");
  size_t max_log_file_size = 1024 * 1024;
  size_t max_log_files = cmdl.get<int>("max_files");

#ifdef _WIN32
  std::string log_file_path = ".\\log\\";
#else
  std::string log_file_path = "/tmp/log/";
#endif

  applog::Logger &logger = applog::Logger::instance();
  logger.setLevel(applog::kTrace);
  log_msg = logger.initFile(log_file_path, log_file_name, max_log_file_size, max_log_files, log_file_suffix);
  if (log_msg) {
    LOG_INFO("Log file path: {}\n", logger.filePath());
  }

  uint16_t udp_server_port = cmdl.get<int>("udp_port");
  uint16_t tcp_server_port = cmdl.get<int>("tcp_port");
#if ENABLE_WEB_SERVER != 0
  uint16_t web_server_port = cmdl.get<int>("web_port");
#endif
  disp_msg = cmdl.exist("disp_msg");
  LOG_INFO("Display message: {}\n", disp_msg);

  // State the web server type and port up front: the type is a build time
  // choice, so the startup log is the only place it can be seen.
#if ENABLE_WEB_SERVER == 1
  LOG_INFO("Web server: WebSocket mode on port {}\n", web_server_port);
#elif ENABLE_WEB_SERVER == 2
  LOG_INFO("Web server: SSE mode on port {}\n", web_server_port);
#else
  LOG_INFO("Web server: disabled\n");
#endif

  try {
    asio::io_context io_context;

#if ENABLE_WEB_SERVER == 1
    Websocket_server websocket_server;
#elif ENABLE_WEB_SERVER == 2
    Sse_server sse_server;
#endif

    Tcp_server tcp_server(io_context, tcp_server_port);

#if ENABLE_WEB_SERVER == 1
    Udp_server udp_server(io_context, udp_server_port, [&tcp_server, &websocket_server, &logger](const std::string &msg) {
#elif ENABLE_WEB_SERVER == 2
    Udp_server udp_server(io_context, udp_server_port, [&tcp_server, &sse_server, &logger](const std::string &msg) {
#else
    Udp_server udp_server(io_context, udp_server_port, [&tcp_server, &logger](const std::string &msg) {
#endif
      if (disp_msg) {
        std::cout << msg << std::flush;
      }
      if (log_msg) {
        logger.writeRaw(msg.data(), msg.size());
      }
      tcp_server.broadcast_message(msg);
#if ENABLE_WEB_SERVER == 1
      if (websocket_server.is_listening()) {
        websocket_server.broadcast_message(msg);
      }
#elif ENABLE_WEB_SERVER == 2
      if (sse_server.is_listening()) {
        sse_server.broadcast_message(msg);
      }
#endif
    });

#if ENABLE_WEB_SERVER == 1
    thread websocket_proc_thread([&] {
      websocket_server.process_messages();
      LOG_INFO("Websocket process thread exit.\n");
    });

    thread websocket_main_thread([&] {
      try {
        websocket_server.run(web_server_port);
      } catch (websocketpp::exception const &e) {
        LOG_ERR("Websocket server exception: {}\n", e.what());
      }
      LOG_INFO("Websocket main thread exit.\n");
    });
#elif ENABLE_WEB_SERVER == 2
    std::thread sse_main_thread([&] {
      try {
        sse_server.run(web_server_port);
      } catch (std::exception const &e) {
        LOG_ERR("SSE server exception: {}\n", e.what());
      }
      LOG_INFO("SSE main thread exit.\n");
    });
#endif

    asio::signal_set signals(io_context, SIGINT);
    signals.async_wait([&](std::error_code /*ec*/, int /*signo*/) {
      LOG_CRITICAL("SIGINT received, stopping server...\n");
      udp_server.stop();
      tcp_server.stop();
      io_context.stop();
#if ENABLE_WEB_SERVER == 1
      websocket_server.stop();
#elif ENABLE_WEB_SERVER == 2
      sse_server.stop();
#endif
    });

    LOG_INFO("TCP server is running on port {}\n", tcp_server_port);
    LOG_INFO("UDP server is running on port {}\n", udp_server_port);
    io_context.run();
    LOG_INFO("TCP server stopped.\n");
    LOG_INFO("UDP server stopped.\n");

#if ENABLE_WEB_SERVER == 1
    websocket_proc_thread.join();
    websocket_main_thread.join();
    LOG_INFO("Websocket server stopped.\n");
#elif ENABLE_WEB_SERVER == 2
    sse_main_thread.join();
    LOG_INFO("SSE server stopped.\n");
#endif
  } catch (std::exception &e) {
    LOG_ERR("Log server exception: {}\n", e.what());
  }
  
  LOG_CRITICAL("=========={} Stop==========\n", app_name);
  
  return 0;
}
