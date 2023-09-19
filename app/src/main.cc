/*
 * Leid 2023.9
 */
#include "cmdline.h"
#include "tcp_server.h"
#include "bl/socket.h"
#include "bl/msgq.h"
#include "common.h"
#include <signal.h>
#include <memory>
#include <thread>
#include <string>
#include <iostream>

using sptrReceiveMsg = std::shared_ptr<std::string>;
bl::MsgQ<sptrReceiveMsg> receiveMsgQ{128};

static volatile bool is_running = true;
static bool disp_msg = true;
static TcpServer server;

static void sighandler(int sig_no) {
  is_running = false;
}

static void sigpipehandler(int sig_no) {
  LOG_WARN("Caught signal SIGPIPE {}", sig_no);
}

static void onIncomingMsg(const std::string &ip, const char *msg, size_t size) {
  LOG_INFO("Receive client({}) msg({}):\n{}", ip, size, msg);
}

static void onClientDisconnected(const std::string &ip, const std::string &msg) {
  LOG_INFO("Client({}) disconnected: {}", ip, msg);
}

static void acceptConnection() {
  while(is_running) {
    try {
      LOG_INFO("Waiting for incoming client...");
      std::string clientIP = server.acceptClient(0);
      LOG_INFO("Accepted new client with IP: {}", clientIP);
      LOG_INFO("== Updated list of accepted clients ==");
      server.printClients();

      char time[128];
      bl::timenow(time);
      std::string welocmeInfo = std::string(time) + " Welcome to LogServer(" APP_VERSION_STR ")! Your IP is " + clientIP + "\n";
      server.sendToClient(clientIP, welocmeInfo.c_str(), welocmeInfo.size());
    } catch (const std::runtime_error &error) {
      if (is_running) {
        LOG_ERR("Accepting client failed: {}", error.what());
      } else {
        LOG_INFO("Accept connection thread exit: {}", error.what());
        break;
      }
    }
  }
  LOG_INFO("acceptConnection() exit!");
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
        pipe_ret_t sendingResult = server.sendToAllClients(msg.data(), msg.size());
        if (!sendingResult.isSuccessful()) {
          LOG_ERR("failed to sent message: {}", sendingResult.message());
        }
      }
    }
  }
  LOG_INFO("processReceiveMsg() exit!");
}

int main(int argc, char *argv[]) {

  cmdline::parser cmdl;
  cmdl.add<int>("udp port", 'u', "UDP port number", false, 11000, cmdline::range(1, 65535));
  cmdl.add<int>("tcp port", 't', "TCP port number", false, 11000, cmdline::range(1, 65535));
  cmdl.add("disp_msg", '\0', "display receive message");
  cmdl.parse_check(argc, argv);

  LOG_CRITICAL("==========LogServer Start==========");
  LOG_CRITICAL("LogServer App Version: {}", APP_VERSION_STR);
  signal(SIGINT, sighandler);
  signal(SIGPIPE, sigpipehandler);

  std::string logger_name = "LogServer";
  std::string logger_level = "info";
  bl::Log::getInstance().init(logger_name, logger_level);

  uint16_t udp_port = cmdl.get<int>("udp port");
  uint16_t tcp_server_port = cmdl.get<int>("tcp port");;
  disp_msg = cmdl.exist("disp_msg");

  LOG_INFO("UDP port: {}", udp_port);
  LOG_INFO("TCP Server port: {}", tcp_server_port);
  LOG_INFO("Display message: {}", disp_msg);

  int err, len;
  char recv_buff[1024];

  bl::Socket udp_socket(udp_port);
  err = udp_socket.Bind(bl::Socket::UDP);
  if (err < 0) {
    LOG_ERR("UDP bind to port {} error!", udp_port);
    return -1;
  }

  pipe_ret_t startRet = server.start(tcp_server_port);
  if (startRet.isSuccessful()) {
    LOG_INFO("TCP Server setup succeeded.");
  } else {
    LOG_ERR("TCP Server setup failed: {}", startRet.message());
    return -1;
  }

  server_observer_t observer;
  observer.incomingPacketHandler = onIncomingMsg;
  observer.disconnectionHandler = onClientDisconnected;
  observer.wantedIP = ""; // use empty string instead to receive messages from any IP address
  server.subscribe(observer);

  std::thread accept_thread(acceptConnection);
  std::thread process_thread(processReceiveMsg);

  accept_thread.detach();
  process_thread.detach();

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
  pipe_ret_t sendingResult = server.close();
  if (sendingResult.isSuccessful()) {
    LOG_INFO("closed server successfully");
  } else {
    LOG_ERR("failed to close server: {}", sendingResult.message());
  }
  LOG_CRITICAL("==========LogServer Stop==========");

  return 0;
}