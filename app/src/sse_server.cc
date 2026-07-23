/*
 * Leid 2026.7
 */
#include "sse_server.hh"

namespace {
    constexpr char SSE_RESPONSE_HEADERS[] = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
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
    } catch (const std::exception& e) {
        LOG_ERR("SSE server exception: {}\n", e.what());
    }
}

void Sse_server::stop() {
    if (!is_running_.load()) return;
    is_running_ = false;
    is_listening_ = false;

    acceptor_.close();

    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& weak_client : clients_) {
        if (auto client = weak_client.lock()) {
            client->close();
        }
    }
    clients_.clear();

    io_context_.stop();
    LOG_INFO("SSE server stopped.\n");
}

void Sse_server::start_accept() {
    auto client = std::make_shared<SseClient>(io_context_);
    
    acceptor_.async_accept(client->socket,
        [this, client](const std::error_code& ec) {
            if (!ec) {
                auto buffer = std::make_shared<asio::streambuf>();
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

void Sse_server::handle_read_request(std::shared_ptr<SseClient> client,
                                      std::shared_ptr<asio::streambuf> buffer,
                                      const std::error_code& ec,
                                      std::size_t /*bytes_transferred*/) {
    if (ec) {
        LOG_ERR("SSE read request error: {}\n", ec.message());
        return;
    }

    std::istream stream(buffer.get());
    std::string request_line;
    if (!std::getline(stream, request_line)) {
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        asio::write(client->socket, asio::buffer(response));
        client->close();
        return;
    }

    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    if (request_line.compare(0, 4, "GET ") != 0) {
        std::string response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        asio::write(client->socket, asio::buffer(response));
        client->close();
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
        client->is_sse_connection = true;
        send_sse_headers(client);
        LOG_INFO("SSE client connected from {}\n", client->socket.remote_endpoint().address().to_string());
    } else if (resource == "/" || resource == "/index.html") {
        send_http_page(client, get_sse_log_page_html(), "text/html");
    } else {
        send_404(client, resource);
    }
}

void Sse_server::send_sse_headers(std::shared_ptr<SseClient> client) {
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.push_back(client);
    }

    asio::async_write(client->socket, asio::buffer(SSE_RESPONSE_HEADERS),
        [this, client](const std::error_code& ec, std::size_t /*bytes*/) {
            if (!ec) {
                send_heartbeat(client);
            } else {
                remove_client(client);
            }
        });
}

void Sse_server::send_http_page(std::shared_ptr<SseClient> client, const std::string& body, const std::string& content_type) {
    auto response = std::make_shared<std::string>();
    *response = "HTTP/1.1 200 OK\r\n";
    *response += "Content-Type: " + content_type + "\r\n";
    *response += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    *response += "Connection: close\r\n\r\n";
    *response += body;

    asio::async_write(client->socket, asio::buffer(*response),
        [client, response](const std::error_code& ec, std::size_t /*bytes*/) {
            client->close();
        });
}

void Sse_server::send_404(std::shared_ptr<SseClient> client, const std::string& resource) {
    auto response = std::make_shared<std::string>();
    *response = "<!DOCTYPE html><title>Error 404</title><h1>Error 404</h1><p>Resource not found: ";
    *response += resource;
    *response += "</p>";

    std::string header = "HTTP/1.1 404 Not Found\r\n";
    header += "Content-Type: text/html\r\n";
    header += "Content-Length: " + std::to_string(response->size()) + "\r\n";
    header += "Connection: close\r\n\r\n";
    *response = header + *response;

    asio::async_write(client->socket, asio::buffer(*response),
        [client, response](const std::error_code& ec, std::size_t /*bytes*/) {
            client->close();
        });
}

void Sse_server::send_heartbeat(std::shared_ptr<SseClient> client) {
    if (!is_running_.load()) return;

    auto heartbeat = std::make_shared<std::string>(format_sse_event("[heartbeat]"));

    asio::async_write(client->socket, asio::buffer(*heartbeat),
        [this, client, heartbeat](const std::error_code& ec, std::size_t /*bytes*/) {
            if (ec) {
                remove_client(client);
                return;
            }

            client->timer.expires_after(std::chrono::seconds(SSE_HEARTBEAT_INTERVAL_SECONDS));
            client->timer.async_wait(
                [this, client](const std::error_code& timer_ec) {
                    if (!timer_ec) {
                        send_heartbeat(client);
                    }
                });
        });
}

void Sse_server::broadcast_message(const std::string& message) {
    if (message.empty()) return;

    std::vector<std::weak_ptr<SseClient>> clients_to_remove;

    auto sse_data = std::make_shared<std::string>(format_sse_event(message));

    {
        std::lock_guard<std::mutex> lock(clients_mutex_);

        if (clients_.empty()) return;

        for (auto& weak_client : clients_) {
            if (auto client = weak_client.lock()) {
                if (!client->is_sse_connection) {
                    continue;
                }

                std::error_code ec;
                asio::write(client->socket, asio::buffer(*sse_data), ec);
                if (ec) {
                    clients_to_remove.push_back(weak_client);
                }
            } else {
                clients_to_remove.push_back(weak_client);
            }
        }
    }

    if (!clients_to_remove.empty()) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (auto it = clients_.begin(); it != clients_.end(); ) {
            bool should_remove = false;
            for (const auto& to_remove : clients_to_remove) {
                if (!it->owner_before(to_remove) && !to_remove.owner_before(*it)) {
                    should_remove = true;
                    break;
                }
            }
            if (should_remove) {
                it = clients_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

void Sse_server::remove_client(std::weak_ptr<SseClient> client) {
    auto client_ptr = client.lock();
    if (!client_ptr) return;

    std::lock_guard<std::mutex> lock(clients_mutex_);

    for (auto it = clients_.begin(); it != clients_.end(); ) {
        if (it->expired()) {
            it = clients_.erase(it);
        } else if (it->owner_before(client) || client.owner_before(*it)) {
            ++it;
        } else {
            if (auto c = it->lock()) {
                c->close();
            }
            it = clients_.erase(it);
            LOG_INFO("SSE client removed\n");
        }
    }
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
