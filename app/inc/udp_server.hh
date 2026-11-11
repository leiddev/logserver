// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 *
 * UDP listener that receives log datagrams from the producers and hands each
 * one to a callback, which forwards it to the TCP clients, the web front end
 * and the log file.
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
#include <vector>

#include "common.h"

//----------------------------------------------------------------------
// UDP server: listens on the UDP port and passes every received datagram
// to the callback, which fans it out to the TCP clients and the web pages.
//----------------------------------------------------------------------

class Udp_server {
 public:
  Udp_server(asio::io_context& io_context, unsigned short udp_port,
             std::function<void(const std::string&)> broadcast_cb)
      : socket_(io_context, asio::ip::udp::endpoint(asio::ip::udp::v4(), udp_port)),
        data_(kMaxDatagramSize),
        broadcast_cb_(broadcast_cb),
        stopped_(false) {
    start_receive();
  }

  // Cancel the pending receive and close the socket. The cancelled handler
  // still runs and would otherwise arm another receive on a dead socket.
  void stop() {
    stopped_ = true;
    socket_.cancel();
    socket_.close();
  }

 private:
  // A UDP datagram that does not fit in the receive buffer is truncated by the
  // kernel and the rest is discarded without any error, so the buffer has to
  // cover the largest payload possible: 65507 bytes for IPv4. Buffers on the
  // heap, not on the stack, because the server lives in main().
  enum { kMaxDatagramSize = 65536 };

  void start_receive() {
    socket_.async_receive_from(asio::buffer(data_), remote_endpoint_,
                               [this](std::error_code ec, std::size_t bytes_recvd) {
                                 if (stopped_)
                                   return;
                                 if (!ec) {
                                   // An empty datagram carries no log line; it is
                                   // not an error, so it is dropped quietly.
                                   if (bytes_recvd > 0) {
                                     if (bytes_recvd == data_.size()) {
                                       LOG_ERR("UDP datagram of {} bytes fills the receive buffer, it may be "
                                               "truncated\n",
                                               bytes_recvd);
                                     }
                                     std::string message(data_.data(), bytes_recvd);
                                     // Hand the datagram to the owner, which fans
                                     // it out to TCP clients, web pages and file.
                                     if (broadcast_cb_) {
                                       broadcast_cb_(message);
                                     }
                                   }
                                 } else if (ec != asio::error::operation_aborted) {
                                   LOG_ERR("UDP receive error: {}\n", ec.message());
                                 }
                                 start_receive();
                               });
  }

  asio::ip::udp::socket socket_;
  asio::ip::udp::endpoint remote_endpoint_;
  std::vector<char> data_;
  std::function<void(const std::string&)> broadcast_cb_;
  bool stopped_;
};

#endif  // !_UDP_SERVER_HH_
