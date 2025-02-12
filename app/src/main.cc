/*
 * Leid 2023.9
 */
#define ENABLE_WEB_SERVER

#include "cmdline.h"
#include "tcp_server.hh"
#include "udp_server.hh"
#ifdef ENABLE_WEB_SERVER
#include "websocket_server.hh"
#endif
#include "common.h"

#include <memory>
#include <thread>
#include <string>
#include <iostream>
#include <cerrno>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/statfs.h>
#include <linux/magic.h>
#endif

static bool disp_msg = true;
static bool log_msg = false;

#ifdef _WIN32
static int mkpath(std::string &s) {
  size_t pre = 0, pos;
  std::string dir;
  int mdret = 0;

  if (s[s.size() - 1] != '\\') {
    // force trailing \ so we can handle everything in loop
    s += '\\';
  }

  while ((pos = s.find_first_of('\\', pre)) != std::string::npos) {
    dir = s.substr(0, pos++);
    pre = pos;
    if (dir.size() == 0)
      continue;  // if leading \ first time is 0 length

    int num = MultiByteToWideChar(CP_ACP, 0, dir.c_str(), -1, NULL, 0);
    wchar_t *wide = new wchar_t[num];
    MultiByteToWideChar(CP_ACP, 0, dir.c_str(), -1, wide, num);
    std::wstring wcpath(wide);
    delete[] wide;

    mdret = CreateDirectoryW(wcpath.c_str(), NULL);
    if ((mdret == 0) && (GetLastError() != ERROR_ALREADY_EXISTS))
      break;
  }

  if ((mdret == 0) && (GetLastError() != ERROR_ALREADY_EXISTS)) {
    ::perror("CreateDirectoryW");
    return -1;
  } else
    return 0;
}
#else
static long getFileSystemType(std::string mount_path) {
  struct statfs fs_info;
  memset(&fs_info, 0, sizeof(fs_info));

  if (statfs(mount_path.c_str(), &fs_info)) {
    ::perror("statfs");
    return -1;
  }

  return fs_info.f_type;
}

static int mkpath(std::string &s, mode_t mode) {
  size_t pre = 0, pos;
  std::string dir;
  int mdret;

  if (s[s.size() - 1] != '/') {
    // force trailing / so we can handle everything in loop
    s += '/';
  }

  while ((pos = s.find_first_of('/', pre)) != std::string::npos) {
    dir = s.substr(0, pos++);
    pre = pos;
    if (dir.size() == 0)
      continue;  // if leading / first time is 0 length
    if ((mdret = ::mkdir(dir.c_str(), mode)) && (errno != EEXIST))
      break;
  }

  if (mdret && (errno != EEXIST)) {
    ::perror("mkdir");
    return -1;
  } else
    return 0;
}
#endif

int main(int argc, char *argv[]) {
  const std::string app_name{"LogServer"};

  cmdline::parser cmdl;
  cmdl.add<int>("udp port", 'u', "UDP port number", false, 11000, cmdline::range(1, 65535));
  cmdl.add<int>("tcp port", 't', "TCP port number", false, 11000, cmdline::range(1, 65535));
  cmdl.add<int>("max files", 's', "Maximum number of files", false, 100, cmdline::range(1, 1000));
  cmdl.add("disp_msg", '\0', "display receive message");
#ifdef ENABLE_WEB_SERVER
  cmdl.add<int>("web port", 'w', "Web port number", false, 11001, cmdline::range(1, 65535));
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

  std::string logger_name = app_name;
  std::string logger_level = "trace";
  std::string log_file_name = "logserver.txt";
  std::string log_file_path;
  size_t max_log_file_size = 1024 * 1024;
  size_t max_log_files = cmdl.get<int>("max files");

#ifdef _WIN32
  log_file_path = ".\\log\\";
  mkpath(log_file_path);
#else
  log_file_path = "/tmp/log/";
  mkpath(log_file_path, 0777);
#endif

#ifdef _WIN32
  if (true) {
#else
  if (getFileSystemType(log_file_path) == TMPFS_MAGIC) {  // temp file system avaliable
#endif
    LOG_INFO("Log file path: {}\n", log_file_path);
    std::string full_path = log_file_path + log_file_name;
    bl::Log::getInstance().init(full_path, logger_name, logger_level, max_log_file_size, max_log_files);
    bl::Log::getInstance().setPattern("%v");
    log_msg = true;
  } else {
    bl::Log::getInstance().init(logger_name, logger_level);
  }

  uint16_t udp_server_port = cmdl.get<int>("udp port");
  uint16_t tcp_server_port = cmdl.get<int>("tcp port");
#ifdef ENABLE_WEB_SERVER
  uint16_t web_server_port = cmdl.get<int>("web port");
#endif
  disp_msg = cmdl.exist("disp_msg");
  LOG_INFO("Display message: {}\n", disp_msg);

  try {
    asio::io_context io_context;

#ifdef ENABLE_WEB_SERVER
    Websocket_server websocket_server;
#else
    int websocket_server; //keep complier happy
#endif

    Tcp_server tcp_server(io_context, tcp_server_port);

    Udp_server udp_server(io_context, udp_server_port, [&tcp_server, &websocket_server](const std::string &msg) {
      if (disp_msg) {
        std::cout << msg << std::flush;
      }
      if (log_msg) {
        LOG_TRACE(msg);
      }
      tcp_server.broadcast_message(msg);
#ifdef ENABLE_WEB_SERVER
      if (websocket_server.is_listening()) {
        websocket_server.broadcast_message(msg);
      }
#endif
    });

#ifdef ENABLE_WEB_SERVER
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
#endif

    asio::signal_set signals(io_context, SIGINT);
    signals.async_wait([&](std::error_code /*ec*/, int /*signo*/) {
      LOG_CRITICAL("SIGINT received, stopping server...\n");
      udp_server.stop();
      tcp_server.stop();
      io_context.stop();
#ifdef ENABLE_WEB_SERVER
      websocket_server.stop();
#endif
    });

    LOG_INFO("TCP server is running on port {}\n", tcp_server_port);
    LOG_INFO("UDP server is running on port {}\n", udp_server_port);
    io_context.run();
    LOG_INFO("TCP server stopped.\n");
    LOG_INFO("UDP server stopped.\n");

#ifdef ENABLE_WEB_SERVER
    websocket_proc_thread.join();
    websocket_main_thread.join();
    LOG_INFO("Websocket server stopped.\n");
#endif
  } catch (std::exception &e) {
    LOG_ERR("Log server exception: {}\n", e.what());
  }
  
  LOG_CRITICAL("=========={} Stop==========\n", app_name);
  
  return 0;
}