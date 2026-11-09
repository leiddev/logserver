// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 */
#pragma once
#ifndef _UDP_SERVER_HH_
#define _UDP_SERVER_HH_

#ifdef _MSC_VER
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0601
  #endif  // !_WIN32_WINNT
#endif

#include <asio.hpp>
#include <functional>
#include <iostream>
#include <string>

#include "common.h"

//----------------------------------------------------------------------
// UDP 服务器类：负责监听UDP端口，接收消息后转发给所有TCP客户端
//----------------------------------------------------------------------

class Udp_server {
 public:
  Udp_server(asio::io_context& io_context, unsigned short udp_port,
             std::function<void(const std::string&)> broadcast_cb)
      : socket_(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), udp_port)), broadcast_cb_(broadcast_cb) {
    start_receive();
  }

  // 停止 UDP 服务，取消异步接收并关闭 socket
  void stop() {
    // 取消所有异步操作
    socket_.cancel();
    // 关闭 UDP socket
    socket_.close();
  }

 private:
  void start_receive() {
    socket_.async_receive_from(asio::buffer(data_, max_length), remote_endpoint_,
                               [this](std::error_code ec, std::size_t bytes_recvd) {
                                 if (!ec && bytes_recvd > 0) {
                                   std::string message(data_, bytes_recvd);
                                   //LOG_INFO("Received UDP message({}): {}\n", message.size(), message);
                                   // 转发UDP消息给所有TCP客户端
                                   if (broadcast_cb_) {
                                     broadcast_cb_(message);
                                   }
                                 } else {
                                   LOG_ERR("UDP receive error: {}\n", ec.message());
                                 }
                                 start_receive();
                               });
  }

  asio::ip::udp::socket socket_;
  asio::ip::udp::endpoint remote_endpoint_;
  enum { max_length = 1024 };
  char data_[max_length];
  std::function<void(const std::string&)> broadcast_cb_;
};

#endif  // !_UDP_SERVER_HH_
