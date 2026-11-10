// SPDX-License-Identifier: MIT
/*
 * Leid 2025.2
 *
 * WebSocket front end, used when the server is built with
 * WEBSERVER_TYPE=WEBSOCKET. Serves the log viewer page at "/" and pushes new
 * log lines to every connected browser. The stream is one way: data frames a
 * client sends are dropped, only the control frames (ping, pong, close) that
 * the protocol needs are handled.
 */
#pragma once
#ifndef _WEBSOCKET_SERVER_HH_
#define _WEBSOCKET_SERVER_HH_

// The ASIO_STANDALONE define is necessary to use the standalone version of Asio.
// Remove if you are using Boost Asio.
#ifndef ASIO_STANDALONE
  #define ASIO_STANDALONE
#endif

#ifndef _WEBSOCKETPP_CPP11_TYPE_TRAITS_
  #define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#endif

#include <websocketpp/common/thread.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <functional>
#include <atomic>
#include <cstddef>
#include <iostream>
#include <set>

#include "common.h"
#include "html_page.hh"
#include "text_encoding.hh"

typedef websocketpp::server<websocketpp::config::asio> websocketserver;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;

using websocketpp::lib::condition_variable;
using websocketpp::lib::lock_guard;
using websocketpp::lib::mutex;
using websocketpp::lib::thread;
using websocketpp::lib::unique_lock;

// Work items handed from the websocketpp callbacks to the processing thread:
// SUBSCRIBE/UNSUBSCRIBE track the client set, BROADCAST sends a log line to
// every client, STOP shuts the server down.
enum action_type { SUBSCRIBE, UNSUBSCRIBE, STOP, BROADCAST };

struct action {
  action(action_type t, connection_hdl h) : type(t), hdl(h) {
  }
  action(action_type t, connection_hdl h, std::string m) : type(t), hdl(h), str(m) {
  }

  action_type type;
  websocketpp::connection_hdl hdl;
  const std::string str;
};

//----------------------------------------------------------------------
// Websocket_server: owns the websocketpp endpoint, the set of subscribers,
// and the thread that drains the action queue.
//----------------------------------------------------------------------
class Websocket_server {
 public:
  Websocket_server() {
    // Enable websocketpp logging, but keep the per frame noise out of it.
    m_server.set_error_channels(websocketpp::log::elevel::all);
    m_server.set_access_channels(websocketpp::log::alevel::all);
    m_server.clear_access_channels(websocketpp::log::alevel::frame_payload);
    m_server.clear_access_channels(websocketpp::log::alevel::frame_header);

    // Initialize Asio Transport
    m_server.init_asio();
    m_server.set_reuse_addr(true);

    // Register handler callbacks. No message handler is registered: the log
    // stream is one way, and websocketpp drops data frames when no handler is
    // set. Control frames (ping, pong, close) are handled by the library
    // itself, so the keepalive and the closing handshake keep working.
    m_server.set_open_handler(bind(&Websocket_server::on_open, this, ::_1));
    m_server.set_close_handler(bind(&Websocket_server::on_close, this, ::_1));
    m_server.set_fail_handler(bind(&Websocket_server::on_fail, this, ::_1));
    m_server.set_http_handler(bind(&Websocket_server::on_http, this, ::_1));
  }

  // Bind, listen and run the IO loop until stop() is called.
  void run(uint16_t port) {
    // Bind IPv4 explicitly. websocketpp's listen(port) binds an IPv6 endpoint
    // and accepts IPv4 clients as IPv4-mapped connections; writing the HTTP or
    // handshake response over those stalls on some Linux hosts, which leaves
    // the browser hanging. Tcp_server and Sse_server listen on IPv4 as well.
    m_server.listen(asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port));

    // Published here, read from the UDP thread through is_listening(); the
    // endpoint's own state must not be touched from outside its IO thread.
    m_listening.store(true);

    // Start the server accept loop
    m_server.start_accept();

    // Run the Asio IO loop until the endpoint is stopped
    try {
      LOG_INFO("Websocket server running on port {}\n", port);
      m_server.run();
    } catch (websocketpp::exception const& e) {
      LOG_ERR("Websocket server exception: {} {}\n", e.code().value(), e.what());
    } catch (const std::exception& e) {
      LOG_ERR("Websocket server exception: {}\n", e.what());
    } catch (...) {
      LOG_ERR("Websocket server: other exception\n");
    }
  }

  // Drop anything still queued and stop the processing thread.
  void stop() {
    {
      lock_guard<mutex> guard(m_action_lock);
      connection_hdl hdl;
      std::queue<action>().swap(m_actions);
      m_actions.push(action(STOP, hdl));
    }
    m_action_cond.notify_one();
  }

  // Queue one log line for every subscriber. The subscriber set belongs to
  // the processing thread, so it must not be looked at here; the count is
  // published separately to keep the common "nobody is watching" case from
  // waking the processing thread for every log line.
  void broadcast_message(const std::string& message) {
    if (m_subscriber_count.load(std::memory_order_relaxed) == 0) {
      return;
    }

    {
      lock_guard<mutex> guard(m_action_lock);
      connection_hdl hdl;
      m_actions.push(action(BROADCAST, hdl, message));
    }
    m_action_cond.notify_one();
  }

  // websocketpp callback: a browser connected.
  void on_open(connection_hdl hdl) {
    {
      lock_guard<mutex> guard(m_action_lock);
      m_actions.push(action(SUBSCRIBE, hdl));
      LOG_INFO("Websocket client on_open\n");
    }
    m_action_cond.notify_one();
  }

  // websocketpp callback: a browser went away.
  void on_close(connection_hdl hdl) {
    {
      lock_guard<mutex> guard(m_action_lock);
      m_actions.push(action(UNSUBSCRIBE, hdl));
      LOG_INFO("Websocket client on_close\n");
    }
    m_action_cond.notify_one();
  }

  // websocketpp callback: the handshake or the transport failed.
  void on_fail(connection_hdl hdl) {
    websocketserver::connection_ptr con = m_server.get_con_from_hdl(hdl);
    if (con->get_ec() != asio::error::operation_aborted) {
      LOG_ERR("Websocket client failed: {} {}\n", con->get_ec().value(), con->get_ec().message());
    }
  }

  // Plain HTTP request: serve the viewer page at "/", 404 for anything else.
  void on_http(connection_hdl hdl) {
    auto con = m_server.get_con_from_hdl(hdl);
    std::string resource = con->get_resource();

    if (resource == "/") {
      std::string body = get_ws_log_page_html();
      con->append_header("Content-Type", "text/html; charset=utf-8");
      con->set_body(body);
      con->set_status(websocketpp::http::status_code::ok);
    } else {
      con->set_status(websocketpp::http::status_code::not_found);
      auto error_page = R"xxx(
<!DOCTYPE html>
<title>Error 404 (Resource not found)</title>
<h1>Error 404</h1>
<p>The requested file <code>
)xxx" + resource + R"xxx(
</code> was not found.</p>
)xxx";
      con->set_body(error_page);
    }
  }

  // Start the closing handshake for every subscriber and forget them.
  void close_all_clients() {
    for (auto& hdl : m_connections) {
      websocketpp::lib::error_code ec;
      m_server.close(hdl, websocketpp::close::status::going_away, "Server shuting down", ec);
      if (ec) {
        LOG_ERR("Websocket client exception: {} {}\n", ec.value(), ec.message());
      }
    }
    con_list().swap(m_connections);
    m_subscriber_count.store(0, std::memory_order_relaxed);
  }

  bool is_listening() {
    return m_listening.load();
  }

  // Processing thread body: drain the action queue until STOP arrives.
  // Sending happens here, never in a websocketpp callback, so a slow client
  // cannot stall the IO thread.
  void process_messages() {
    bool is_running = true;

    while (is_running) {
      unique_lock<mutex> lock(m_action_lock);

      while (m_actions.empty()) {
        m_action_cond.wait(lock);
      }

      action a = m_actions.front();
      m_actions.pop();

      lock.unlock();

      if (a.type == SUBSCRIBE) {
        lock_guard<mutex> guard(m_connection_lock);
        m_connections.insert(a.hdl);
        m_subscriber_count.store(m_connections.size(), std::memory_order_relaxed);
      } else if (a.type == UNSUBSCRIBE) {
        lock_guard<mutex> guard(m_connection_lock);
        m_connections.erase(a.hdl);
        m_subscriber_count.store(m_connections.size(), std::memory_order_relaxed);
      } else if (a.type == STOP) {
        lock_guard<mutex> guard(m_connection_lock);
        // The exception free overloads keep the error inside this branch: an
        // exception here used to unlock m_connection_lock a second time and
        // leave is_running set, which hung the thread and the join in main().
        if (m_listening.exchange(false)) {
          websocketpp::lib::error_code ec;
          m_server.stop_listening(ec);
          if (ec) {
            LOG_ERR("Websocket stop_listening failed: {} {}\n", ec.value(), ec.message());
          }
        }
        close_all_clients();
        is_running = false;
      } else if (a.type == BROADCAST) {
        lock_guard<mutex> guard(m_connection_lock);
        // A text frame must be valid UTF-8, so convert once for all clients.
        // toDisplayUtf8 never fails, which is what keeps a malformed line from
        // silently disappearing the way the old GB2312 retry did.
        const std::string body = textenc::toDisplayUtf8(a.str);
        for (auto& hdl : m_connections) {
          try {
            m_server.send(hdl, body, websocketpp::frame::opcode::text);
          } catch (websocketpp::exception const& e) {
            LOG_ERR("Websocket client exception: {} {}\n", e.code().value(), e.what());
          }
        }
      }
    }
  }

 private:
  typedef std::set<connection_hdl, std::owner_less<connection_hdl> > con_list;

  websocketserver m_server;
  con_list m_connections;
  std::queue<action> m_actions;

  // Set once listen() succeeded, cleared when the endpoint stops listening.
  // Written on the IO thread, read from the UDP thread through is_listening().
  std::atomic<bool> m_listening{false};

  // Number of entries in m_connections, published for broadcast_message().
  std::atomic<std::size_t> m_subscriber_count{0};

  mutex m_action_lock;
  mutex m_connection_lock;
  condition_variable m_action_cond;
};

#endif  // !_WEBSOCKET_SERVER_HH_
