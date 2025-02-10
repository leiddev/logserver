/*
 * Leid 2023.9
 */
#include "cmdline.h"
#include "tcp_server.h"
#include "websocket_server.hh"
#include "bl/socket.h"
#include "bl/msgq.h"
#include "bl/time.h"
#include "common.h"
#include <memory>
#include <thread>
#include <string>
#include <iostream>
#include <cerrno>
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <linux/magic.h>
#endif

using sptrReceiveMsg = std::shared_ptr<std::string>;
bl::MsgQ<sptrReceiveMsg> receiveMsgQ{128};

static volatile bool is_running = true;
static bool disp_msg = true;
bool log_msg = false;
static TcpServer tcp_server;

#ifdef _WIN32
static BOOL WINAPI CtrlHandler(DWORD dwControlType) {
  switch (dwControlType) {
    case CTRL_C_EVENT:
    case CTRL_CLOSE_EVENT:
      is_running = false;
      return TRUE;
    default:
      return FALSE;
  }
}
#else
static void sighandler(int sig_no) {
  is_running = false;
}

static void sigpipehandler(int sig_no) {
  LOG_WARN("Caught signal SIGPIPE {}", sig_no);
}
#endif

static void onIncomingMsg(const std::string &ip, const char *msg, size_t size) {
  LOG_INFO("Receive client({}) msg({}):\n{}\n", ip, size, msg);
}

static void onClientDisconnected(const std::string &ip, const std::string &msg) {
  LOG_INFO("Client({}) disconnected: {}\n", ip, msg);
}

static void acceptConnection() {
  while(is_running) {
    try {
      LOG_INFO("Waiting for incoming client...\n");
      std::string clientIP = tcp_server.acceptClient(0);
      LOG_INFO("Accepted new client with IP: {}\n", clientIP);
      LOG_INFO("== Updated list of accepted clients ==\n");
      tcp_server.printClients();

      char time[128];
      bl::timenow(time);
      std::string welocmeInfo = std::string(time) + " Welcome to LogServer(" APP_VERSION_STR ")! Your IP is " + clientIP + "\n";
      tcp_server.sendToClient(clientIP, welocmeInfo.c_str(), welocmeInfo.size());
    } catch (const std::runtime_error &error) {
      if (is_running) {
        LOG_ERR("Accepting client failed: {}\n", error.what());
      } else {
        LOG_INFO("Accept connection thread exit: {}\n", error.what());
        break;
      }
    }
  }
  LOG_INFO("acceptConnection() exit!\n");
}

static void processReceiveMsg() {
  while(is_running) {
    if (0 == receiveMsgQ.Wait(1000)) {
      sptrReceiveMsg sptrRmsg = receiveMsgQ.Get();
      if (sptrRmsg) {
        std::string msg(sptrRmsg->data(), sptrRmsg->size());
        if (disp_msg) {
          std::cout << msg << std::flush;
        }
        if (log_msg) {
          LOG_TRACE(msg);
        }
        pipe_ret_t sendingResult = tcp_server.sendToAllClients(msg.data(), msg.size());
        if (!sendingResult.isSuccessful()) {
          LOG_ERR("failed to sent message: {}\n", sendingResult.message());
        }
        websocket_send_message(msg);
      }
    }
  }
  LOG_INFO("processReceiveMsg() exit!\n");
}

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
  cmdl.add<int>("web port", 'w', "Web port number", false, 11001, cmdline::range(1, 65535));
  cmdl.parse_check(argc, argv);

  LOG_CRITICAL("=========={} Start==========\n", app_name);
  LOG_CRITICAL("{} App Version: {}\n", app_name, APP_VERSION_STR);

#ifdef _WIN32
  if (!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
    LOG_ERR("error setting event handler.");
  }

#define CODE_PAGE_GBK (936)
#define CODE_PAGE_UTF8 (65001)
  if (IsValidCodePage(CODE_PAGE_UTF8)) {
    SetConsoleCP(CODE_PAGE_UTF8);
    SetConsoleOutputCP(CODE_PAGE_UTF8);
    std::string console_title = app_name + " " + std::string(APP_VERSION_STR);
    SetConsoleTitle(console_title.c_str());
  }

#else
  signal(SIGINT, sighandler);
  signal(SIGPIPE, sigpipehandler);
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

  uint16_t udp_port = cmdl.get<int>("udp port");
  uint16_t tcp_server_port = cmdl.get<int>("tcp port");
  uint16_t web_server_port = cmdl.get<int>("web port");
  disp_msg = cmdl.exist("disp_msg");

  LOG_INFO("UDP port: {}\n", udp_port);
  LOG_INFO("TCP Server port: {}\n", tcp_server_port);
  LOG_INFO("Web Server port: {}\n", web_server_port);
  LOG_INFO("Display message: {}\n", disp_msg);

  int err, len;
  char recv_buff[1024];

  bl::Socket udp_socket(udp_port);
  err = udp_socket.Bind(bl::Socket::UDP);
  if (err < 0) {
    LOG_ERR("UDP bind to port {} error!\n", udp_port);
    return -1;
  }

  pipe_ret_t startRet = tcp_server.start(tcp_server_port);
  if (startRet.isSuccessful()) {
    LOG_INFO("TCP Server setup succeeded.\n");
  } else {
    LOG_ERR("TCP Server setup failed: {}\n", startRet.message());
    return -1;
  }

  server_observer_t observer;
  observer.incomingPacketHandler = onIncomingMsg;
  observer.disconnectionHandler = onClientDisconnected;
  observer.wantedIP = ""; // use empty string instead to receive messages from any IP address
  tcp_server.subscribe(observer);

  std::thread accept_thread(acceptConnection);
  std::thread process_thread(processReceiveMsg);

  websocket_server_start(web_server_port);

  while (is_running) {
    len = udp_socket.Recv((uint8_t *)recv_buff, sizeof(recv_buff));
    if (len > 0) {
      sptrReceiveMsg sptrRmsg = std::make_shared<std::string>(recv_buff, len);
      receiveMsgQ.Add(sptrRmsg);
    }
    bl::sleep(1);
  }

  udp_socket.Close();
  bl::sleep(100);
  pipe_ret_t sendingResult = tcp_server.close();
  accept_thread.join();
  process_thread.join();

  if (sendingResult.isSuccessful()) {
    LOG_INFO("closed server successfully\n");
  } else {
    LOG_ERR("failed to close server: {}\n", sendingResult.message());
  }
  websocket_server_stop();
  LOG_CRITICAL("=========={} Stop==========\n", app_name);
  
  return 0;
}