/*
 * Leid 2025.2
 */
#pragma once
#ifndef _SSE_SERVER_HH_
#define _SSE_SERVER_HH_

#ifdef _MSC_VER
  #ifndef _WIN32_WINNT
    #define _WIN32_WINNT 0x0601
  #endif
#endif

#ifndef ASIO_STANDALONE
  #define ASIO_STANDALONE
#endif

#include <asio.hpp>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>

#include "common.h"
#include "html_page.hh"

//----------------------------------------------------------------------
// SSE 客户端连接结构
//----------------------------------------------------------------------
struct SseClient : public std::enable_shared_from_this<SseClient> {
    asio::ip::tcp::socket socket;
    asio::steady_timer timer;
    bool is_sse_connection;
    
    SseClient(asio::io_context& io_ctx)
        : socket(io_ctx), timer(io_ctx), is_sse_connection(false) {}
    
    void close() {
        timer.cancel();
        std::error_code ec;
        if (socket.is_open()) {
            socket.close(ec);
        }
    }
};

//----------------------------------------------------------------------
// SSE 服务器类
//----------------------------------------------------------------------
class Sse_server {
 public:
    Sse_server();
    ~Sse_server();

    void run(uint16_t port);
    void stop();
    void broadcast_message(const std::string& message);
    bool is_listening() const { return is_listening_.load(); }

 private:
    void start_accept();
    void handle_accept(const std::error_code& ec);
    void handle_read_request(std::shared_ptr<SseClient> client, 
                             std::shared_ptr<asio::streambuf> buffer,
                             const std::error_code& ec, std::size_t bytes_transferred);
    void send_sse_headers(std::shared_ptr<SseClient> client);
    void send_http_page(std::shared_ptr<SseClient> client, const std::string& body, const std::string& content_type);
    void send_404(std::shared_ptr<SseClient> client, const std::string& resource);
    void send_heartbeat(std::shared_ptr<SseClient> client);
    void broadcast_to_all(const std::string& message);
    void remove_client(std::weak_ptr<SseClient> client);

    std::string format_sse_event(const std::string& message);
    std::string get_content_type(const std::string& path);

    asio::io_context io_context_;
    asio::ip::tcp::acceptor acceptor_;
    std::vector<std::weak_ptr<SseClient>> clients_;
    std::mutex clients_mutex_;
    std::atomic<bool> is_listening_;
    std::atomic<bool> is_running_;
};

#endif  // !_SSE_SERVER_HH_
