// SPDX-License-Identifier: MIT
/*
 * Leid 2026.7
 *
 * Implementation of the SSE front end: request parsing, the viewer page and
 * 404 responses, the event stream with its heartbeat, and the client list.
 *
 * Every socket operation happens on the io_context thread. broadcast_message()
 * is called from the thread that receives the UDP datagrams, so it only takes
 * a snapshot of the client list and posts the event to each client: a browser
 * that stops reading fills that client's queue and is dropped, without ever
 * holding up the receiving thread.
 */
#include "sse_server.hh"

#include "text_encoding.hh"

namespace {
    constexpr char SSE_RESPONSE_HEADERS[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "X-Accel-Buffering: no\r\n\r\n";
    
    constexpr char HTTP_OK_PREFIX[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: ";
    
    constexpr char HTTP_NOT_FOUND[] = 
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html\r\n"
        "Content-Length: ";
    
    constexpr char SSE_EVENT_PREFIX[] = "data: ";
    constexpr char SSE_EVENT_SUFFIX[] = "\n\n";
    constexpr int SSE_HEARTBEAT_INTERVAL_SECONDS = 30;

    // How much may be queued for one client before it is considered dead and
    // closed. A browser reading normally never gets near this; one that
    // stopped reading reaches it in a few seconds of busy logging.
    constexpr std::size_t SSE_MAX_QUEUED_BYTES = 4 * 1024 * 1024;

    // A request header is a "GET <path> HTTP/1.1" line and a handful of
    // fields; anything larger is not a browser, and the read stops there
    // instead of growing the buffer until the process runs out of memory.
    constexpr std::size_t SSE_MAX_REQUEST_BYTES = 8 * 1024;

    // A client that connects and then sends nothing, or stops half way
    // through the request line, would otherwise hold the connection forever.
    constexpr int SSE_REQUEST_TIMEOUT_SECONDS = 10;
}

Sse_server::Sse_server()
    : io_context_(),
      acceptor_(io_context_),
      is_listening_(false),
      is_running_(false) {
}

Sse_server::~Sse_server() {
    stop();
}

void Sse_server::run(uint16_t port) {
    try {
        asio::ip::tcp::endpoint endpoint(asio::ip::tcp::v4(), port);
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
        acceptor_.bind(endpoint);
        acceptor_.listen();
        
        is_listening_ = true;
        is_running_ = true;
        
        LOG_INFO("SSE server is running on port {}\n", port);
        start_accept();
        
        io_context_.run();

        // The loop can also end without stop() having posted the shutdown, so
        // close what is still open here; a second call is a no-op.
        shutdown_in_io_thread();
    } catch (const std::exception& e) {
        is_listening_ = false;
        is_running_ = false;
        LOG_ERR("SSE server exception: {}\n", e.what());
    }
}

void Sse_server::stop() {
    if (!is_running_.load()) return;
    is_listening_ = false;

    // The acceptor and the clients belong to the io_context thread, and so
    // does the mutex-protected client list; hand the shutdown over to it.
    asio::post(io_context_, [this] { shutdown_in_io_thread(); });
}

void Sse_server::shutdown_in_io_thread() {
    if (!is_running_.exchange(false)) return;
    is_listening_ = false;

    std::error_code ec;
    acceptor_.close(ec);

    std::vector<std::shared_ptr<SseClient>> clients_to_close;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto& weak_client : clients_) {
            if (auto client = weak_client.lock()) {
                clients_to_close.push_back(client);
            }
        }
        clients_.clear();
    }
    for (auto& client : clients_to_close) {
        client->close();
    }

    io_context_.stop();
    LOG_INFO("SSE server stopped.\n");
}

void Sse_server::start_accept() {
    auto client = std::make_shared<SseClient>(io_context_);
    
    acceptor_.async_accept(client->socket,
        [this, client](const std::error_code& ec) {
            if (!ec) {
                start_request_timeout(client);

                auto buffer = std::make_shared<asio::streambuf>(SSE_MAX_REQUEST_BYTES);
                asio::async_read_until(client->socket, *buffer, "\r\n\r\n",
                    [this, client, buffer](const std::error_code& ec2, std::size_t bytes) {
                        handle_read_request(client, buffer, ec2, bytes);
                    });
            }
            if (is_running_.load()) {
                start_accept();
            }
        });
}

void Sse_server::start_request_timeout(std::shared_ptr<SseClient> client) {
    client->timer.expires_after(std::chrono::seconds(SSE_REQUEST_TIMEOUT_SECONDS));
    client->timer.async_wait(
        [this, client](const std::error_code& timer_ec) {
            if (timer_ec || client->request_received) return;
            LOG_ERR("SSE client sent no request in time, closing it\n");
            remove_client(client);
        });
}

void Sse_server::handle_read_request(std::shared_ptr<SseClient> client,
                                      std::shared_ptr<asio::streambuf> buffer,
                                      const std::error_code& ec,
                                      std::size_t /*bytes_transferred*/) {
    client->request_received = true;
    client->timer.cancel();

    if (ec) {
        // The timeout or the shutdown may have closed the socket first; the
        // read then fails with whatever the platform reports for a dead
        // descriptor, and there is nothing left to answer.
        if (!client->socket.is_open()) return;

        if (ec == asio::error::not_found) {
            // The buffer filled up without ever reaching the blank line that
            // ends the header.
            LOG_ERR("SSE request header exceeds {} bytes, closing the client\n", SSE_MAX_REQUEST_BYTES);
            client->close_after_write = true;
            enqueue(client, "HTTP/1.1 431 Request Header Fields Too Large\r\n\r\n");
        } else if (ec != asio::error::operation_aborted) {
            LOG_ERR("SSE read request error: {}\n", ec.message());
            remove_client(client);
        }
        return;
    }

    std::istream stream(buffer.get());
    std::string request_line;
    if (!std::getline(stream, request_line)) {
        client->close_after_write = true;
        enqueue(client, "HTTP/1.1 400 Bad Request\r\n\r\n");
        return;
    }

    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    if (request_line.compare(0, 4, "GET ") != 0) {
        client->close_after_write = true;
        enqueue(client, "HTTP/1.1 400 Bad Request\r\n\r\n");
        return;
    }

    std::string resource;
    size_t space1 = 4;
    size_t space2 = request_line.find(' ', space1);
    if (space2 != std::string::npos) {
        resource = request_line.substr(space1, space2 - space1);
    } else {
        resource = request_line.substr(space1);
    }

    if (resource == "/sse") {
        send_sse_headers(client);
    } else if (resource == "/" || resource == "/index.html") {
        send_http_page(client, get_sse_log_page_html(), "text/html; charset=utf-8");
    } else {
        send_404(client, resource);
    }
}

void Sse_server::send_sse_headers(std::shared_ptr<SseClient> client) {
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.push_back(client);
    }

    std::error_code ec;
    auto endpoint = client->socket.remote_endpoint(ec);
    if (!ec) {
        LOG_INFO("SSE client connected from {}\n", endpoint.address().to_string());
    }

    // asio::buffer on a char array takes all N bytes, terminator included, so
    // pass the length explicitly: the stray NUL turned the first event field
    // into "\0data" and made EventSource drop the first heartbeat.
    enqueue(client, std::string(SSE_RESPONSE_HEADERS, sizeof(SSE_RESPONSE_HEADERS) - 1));
    enqueue(client, format_sse_event("[heartbeat]"));
    schedule_heartbeat(client);
}

void Sse_server::send_http_page(std::shared_ptr<SseClient> client, const std::string& body, const std::string& content_type) {
    std::string response;
    response = "HTTP/1.1 200 OK\r\n";
    response += "Content-Type: " + content_type + "\r\n";
    response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;

    client->close_after_write = true;
    enqueue(client, response);
}

void Sse_server::send_404(std::shared_ptr<SseClient> client, const std::string& resource) {
    std::string body = "<!DOCTYPE html><title>Error 404</title><h1>Error 404</h1><p>Resource not found: ";
    body += htmlEscape(resource);
    body += "</p>";

    std::string response = HTTP_NOT_FOUND;
    response += std::to_string(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;

    client->close_after_write = true;
    enqueue(client, response);
}

void Sse_server::schedule_heartbeat(std::shared_ptr<SseClient> client) {
    client->timer.expires_after(std::chrono::seconds(SSE_HEARTBEAT_INTERVAL_SECONDS));
    client->timer.async_wait(
        [this, client](const std::error_code& timer_ec) {
            if (timer_ec) return;
            enqueue(client, format_sse_event("[heartbeat]"));
            schedule_heartbeat(client);
        });
}

void Sse_server::enqueue(std::shared_ptr<SseClient> client, std::string data) {
    if (!client || !client->socket.is_open()) return;

    if (client->queued_bytes + data.size() > SSE_MAX_QUEUED_BYTES) {
        LOG_WARN("SSE client is not reading, closing it\n");
        remove_client(client);
        return;
    }

    client->queued_bytes += data.size();
    client->write_queue.push_back(std::move(data));
    if (!client->is_writing) {
        start_write(client);
    }
}

void Sse_server::start_write(std::shared_ptr<SseClient> client) {
    client->is_writing = true;

    // The front of the queue stays alive until the write completes, so the
    // buffer cannot dangle.
    asio::async_write(client->socket, asio::buffer(client->write_queue.front()),
        [this, client](const std::error_code& ec, std::size_t /*bytes*/) {
            if (ec) {
                remove_client(client);
                return;
            }

            client->queued_bytes -= client->write_queue.front().size();
            client->write_queue.pop_front();

            if (!client->write_queue.empty()) {
                start_write(client);
                return;
            }

            client->is_writing = false;
            if (client->close_after_write) {
                remove_client(client);
            }
        });
}

void Sse_server::broadcast_message(const std::string& message) {
    if (message.empty()) return;
    if (!is_running_.load()) return;

    // EventSource always decodes the stream as UTF-8, so a line that arrives
    // in GB2312 has to be converted here or the browser shows mojibake.
    auto sse_data = std::make_shared<const std::string>(format_sse_event(textenc::toDisplayUtf8(message)));

    std::vector<std::shared_ptr<SseClient>> clients;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        for (auto it = clients_.begin(); it != clients_.end(); ) {
            if (auto client = it->lock()) {
                clients.push_back(client);
                ++it;
            } else {
                it = clients_.erase(it);
            }
        }
    }

    // Queueing, and therefore every socket write, belongs to the io_context
    // thread; writing here would block the thread that receives the datagrams
    // as soon as one browser stops reading.
    for (auto& client : clients) {
        asio::post(io_context_, [this, client, sse_data] { enqueue(client, *sse_data); });
    }
}

void Sse_server::remove_client(std::shared_ptr<SseClient> client) {
    if (!client) return;

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto it = clients_.begin(); it != clients_.end(); ) {
            auto existing = it->lock();
            if (!existing) {
                it = clients_.erase(it);
            } else if (existing == client) {
                it = clients_.erase(it);
                LOG_INFO("SSE client removed\n");
                break;
            } else {
                ++it;
            }
        }
    }

    client->close();
}

std::string Sse_server::format_sse_event(const std::string& message) {
    std::string result;

    std::string normalized = message;
    size_t pos = 0;
    while ((pos = normalized.find("\r\n", pos)) != std::string::npos) {
        normalized.erase(pos, 1);
    }

    std::istringstream stream(normalized);
    std::string line;
    while (std::getline(stream, line)) {
        result += SSE_EVENT_PREFIX;
        result += line;
        result += '\n';
    }
    result += SSE_EVENT_SUFFIX;
    return result;
}

std::string Sse_server::get_content_type(const std::string& path) {
    if (path.find(".html") != std::string::npos || path == "/") {
        return "text/html";
    } else if (path.find(".js") != std::string::npos) {
        return "application/javascript";
    } else if (path.find(".css") != std::string::npos) {
        return "text/css";
    } else if (path.find(".json") != std::string::npos) {
        return "application/json";
    }
    return "text/plain";
}
