// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
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
// TCP 会话类：负责处理单个TCP客户端连接
//----------------------------------------------------------------------

class Session : public std::enable_shared_from_this<Session> {
 public:
  explicit Session(asio::ip::tcp::socket socket) : socket_(std::move(socket)), stopped_(false) {
  }

  // 启动会话：获取客户端地址、发送欢迎信息、开始读取数据
  void start() {
    try {
      endpoint_ = socket_.remote_endpoint();
      print_client_info("Connected");
    } catch (const asio::system_error& e) {
      LOG_ERR("TCP server failed to get client endpoint: {}\n", e.what());
      stop();
      return;
    }
    // 发送欢迎信息
    std::string welocme_info = "Welcome to LogServer(" APP_VERSION_STR ")! Your IP is " +
                              endpoint_.address().to_string() + "\n";
    deliver(welocme_info);
    // 开始异步读取数据
    do_read();
  }

  // 设置会话关闭时的回调，用于通知服务器移除该会话
  void set_on_close(std::function<void(std::shared_ptr<Session>)> callback) {
    on_close_ = std::move(callback);
  }

  // 向该会话发送一条消息（无论是echo或UDP广播）
  void deliver(const std::string& msg) {
    auto self(shared_from_this());
    // post到IO上下文保证线程安全（如果有多线程时）
    asio::post(socket_.get_executor(), [this, self, msg]() {
      bool write_in_progress = !write_msgs_.empty();
      write_msgs_.push_back(msg);
      if (!write_in_progress) {
        do_write();
      }
    });
  }

  // 主动关闭连接
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
  // 异步读取客户端数据
  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(asio::buffer(data_, max_length), [this, self](std::error_code ec, std::size_t length) {
      if (!ec) {
        // 将接收到的数据转成字符串，并打印日志
        std::string message(data_, length);
        LOG_INFO("TCP server receive client({}:{}) msg({}):\n{}\n", endpoint_.address().to_string(), endpoint_.port(),
                 message.size(), message);
        // Echo：将收到的数据回送给该客户端
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

  // 异步写操作：将写消息队列中的消息发送出去
  void do_write() {
    auto self(shared_from_this());
    asio::async_write(socket_, asio::buffer(write_msgs_.front().data(), write_msgs_.front().length()),
                      [this, self](std::error_code ec, std::size_t /*length*/) {
                        if (!ec) {
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

  // 打印客户端的地址信息
  void print_client_info(const std::string& status) {
    try {
      LOG_INFO("TCP server's client {}:{} {}\n", endpoint_.address().to_string(), endpoint_.port(), status);
    } catch (const std::exception& e) {
      LOG_ERR("TCP server failed to get client endpoint: {}\n", e.what());
    }
  }

  asio::ip::tcp::socket socket_;
  enum { max_length = 1024 };
  char data_[max_length];
  bool stopped_;
  std::deque<std::string> write_msgs_;
  std::function<void(std::shared_ptr<Session>)> on_close_;
  asio::ip::tcp::endpoint endpoint_;
};

//----------------------------------------------------------------------
// TCP 服务器类：负责接受新的TCP连接，并管理所有会话
//----------------------------------------------------------------------

class Tcp_server {
 public:
  Tcp_server(asio::io_context& io_context, unsigned short tcp_port)
      : io_context_(io_context), acceptor_(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), tcp_port)) {
    do_accept();
  }

  // 将消息广播给所有当前TCP客户端
  void broadcast_message(const std::string& msg) {
    for (auto& s : sessions_) {
      s->deliver(msg);
    }
  }

  // 停止服务器：关闭监听器，并主动关闭所有会话
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