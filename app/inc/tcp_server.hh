// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 *
 * TCP front end for log consumers: accepts several clients, greets each one,
 * and broadcasts every received log line to all of them. Data sent by a
 * client is echoed back and logged.
 */
#pragma once
#ifndef _TCP_SERVER_HH_
#define _TCP_SERVER_HH_

#ifdef _MSC_VER
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0601
  #endif  // !_WIN32_WINNT
#endif

#include <asio.hpp>
#include <csignal>
#include <deque>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>

#include "common.h"

//----------------------------------------------------------------------
// Session: one connected TCP client.
//----------------------------------------------------------------------

class Session : public std::enable_shared_from_this<Session> {
 public:
  explicit Session(asio::ip::tcp::socket socket)
      : socket_(std::move(socket)), stopped_(false), queued_bytes_(0) {
  }

  // Remember the peer address, greet it, and start reading.
  void start() {
    try {
      endpoint_ = socket_.remote_endpoint();
      print_client_info("Connected");
    } catch (const asio::system_error& e) {
      LOG_ERR("TCP server failed to get client endpoint: {}\n", e.what());
      stop();
      return;
    }
    std::string welocme_info = "Welcome to LogServer(" APP_VERSION_STR ")! Your IP is " +
                              endpoint_.address().to_string() + "\n";
    deliver(welocme_info);
    do_read();
  }

  // Register the callback used to remove this session from its server.
  void set_on_close(std::function<void(std::shared_ptr<Session>)> callback) {
    on_close_ = std::move(callback);
  }

  // Queue a message for this client; either an echo or a broadcast.
  //
  // A consumer that stops reading fills this queue and the server grows with
  // every log line, so past kMaxQueuedBytes the session is dropped the same
  // way the SSE and WebSocket front ends drop one. The check runs inside the
  // posted handler, which keeps the queue on the IO thread only.
  void deliver(const std::string& msg) {
    auto self(shared_from_this());
    // Post through the executor so the queue is only touched from the IO thread.
    asio::post(socket_.get_executor(), [this, self, msg]() {
      if (stopped_)
        return;
      if (queued_bytes_ + msg.size() > kMaxQueuedBytes) {
        LOG_WARN("TCP client is not reading, closing it\n");
        stop();
        return;
      }
      bool write_in_progress = !write_msgs_.empty();
      queued_bytes_ += msg.size();
      write_msgs_.push_back(msg);
      if (!write_in_progress) {
        do_write();
      }
    });
  }

  // Close the connection and notify the server.
  void stop() {
    if (stopped_)
      return;
    stopped_ = true;
    std::error_code ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    socket_.close(ec);
    print_client_info("Disconnected");
    if (on_close_) {
      on_close_(shared_from_this());
    }
  }

 private:
  // Read one chunk from the client, log it, and echo it back.
  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(asio::buffer(data_, max_length), [this, self](std::error_code ec, std::size_t length) {
      if (!ec) {
        std::string message(data_, length);
        LOG_INFO("TCP server receive client({}:{}) msg({}):\n{}\n", endpoint_.address().to_string(), endpoint_.port(),
                 message.size(), message);
        deliver(message);
        do_read();
      } else {
        if ((ec != asio::error::operation_aborted) && (ec != asio::error::eof)) {
          LOG_ERR("TCP server read error: {}\n", ec.message());
        }
        stop();
      }
    });
  }

  // Write the queue head, then continue while more messages are pending.
  void do_write() {
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
                          queued_bytes_ -= write_msgs_.front().size();
                          write_msgs_.pop_front();
                          if (!write_msgs_.empty()) {
                            do_write();
                          }
                        } else {
                          if ((ec != asio::error::operation_aborted) && (ec != asio::error::eof)) {
                            LOG_ERR("TCP server write error: {}\n", ec.message());
                          }
                          stop();
                        }
                      });
  }

  // Log the peer address together with the given state word.
  void print_client_info(const std::string& status) {
    try {
      LOG_INFO("TCP server's client {}:{} {}\n", endpoint_.address().to_string(), endpoint_.port(), status);
    } catch (const std::exception& e) {
      LOG_ERR("TCP server failed to get client endpoint: {}\n", e.what());
    }
  }

  asio::ip::tcp::socket socket_;
  enum { max_length = 1024 };
  // Unsent bytes allowed for one client before it is considered gone. Matches
  // the SSE (SSE_MAX_QUEUED_BYTES) and WebSocket (kMaxBufferedBytes) limits.
  enum { kMaxQueuedBytes = 4 * 1024 * 1024 };
  char data_[max_length];
  bool stopped_;
  std::deque<std::string> write_msgs_;
  std::size_t queued_bytes_;
  std::function<void(std::shared_ptr<Session>)> on_close_;
  asio::ip::tcp::endpoint endpoint_;
};

//----------------------------------------------------------------------
// Tcp_server: accepts new clients and owns the set of live sessions.
//----------------------------------------------------------------------

class Tcp_server {
 public:
  Tcp_server(asio::io_context& io_context, unsigned short tcp_port)
      : io_context_(io_context), acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), tcp_port)) {
    do_accept();
  }

  // Send one message to every connected client.
  void broadcast_message(const std::string& msg) {
    for (auto& s : sessions_) {
      s->deliver(msg);
    }
  }

  // Close the acceptor and every live session.
  void stop() {
    std::error_code ec;
    acceptor_.close(ec);
    std::set<std::shared_ptr<Session>> sessions_copy = sessions_;
    for (auto& s : sessions_copy) {
      if (s)
        s->stop();
    }
  }

 private:
  // Accept one client, register it, then wait for the next one.
  void do_accept() {
    acceptor_.async_accept([this](std::error_code ec, asio::ip::tcp::socket socket) {
      if (!ec) {
        auto new_session = std::make_shared<Session>(std::move(socket));
        new_session->set_on_close([this](std::shared_ptr<Session> s) { sessions_.erase(s); });
        sessions_.insert(new_session);
        new_session->start();
      } else {
        if (ec != asio::error::operation_aborted) {
          LOG_ERR("TCP server accept error: {}\n", ec.message());
        }
      }
      if (acceptor_.is_open())
        do_accept();
    });
  }

  asio::io_context& io_context_;
  asio::ip::tcp::acceptor acceptor_;
  std::set<std::shared_ptr<Session>> sessions_;
};

#endif  // !_TCP_SERVER_HH_