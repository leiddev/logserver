/*
 * Leid 2025.2
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
#include <iostream>
#include <set>

#include "common.h"
#include "html_page.hh"

typedef websocketpp::server<websocketpp::config::asio> websocketserver;

using websocketpp::connection_hdl;
using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

using websocketpp::lib::condition_variable;
using websocketpp::lib::lock_guard;
using websocketpp::lib::mutex;
using websocketpp::lib::thread;
using websocketpp::lib::unique_lock;

/* on_open insert connection_hdl into channel
 * on_close remove connection_hdl from channel
 * on_message queue send to all channels
 */
enum action_type { SUBSCRIBE, UNSUBSCRIBE, MESSAGE, STOP, BROADCAST };

struct action {
  action(action_type t, connection_hdl h) : type(t), hdl(h) {
  }
  action(action_type t, connection_hdl h, websocketserver::message_ptr m) : type(t), hdl(h), msg(m) {
  }
  action(action_type t, connection_hdl h, std::string m) : type(t), hdl(h), str(m) {
  }

  action_type type;
  websocketpp::connection_hdl hdl;
  websocketserver::message_ptr msg;
  const std::string str;
};

class Websocket_server {
 public:
  Websocket_server() {
    // Set logging settings
    m_server.set_error_channels(websocketpp::log::elevel::all);
    m_server.set_access_channels(websocketpp::log::alevel::all);
    m_server.clear_access_channels(websocketpp::log::alevel::frame_payload);
    m_server.clear_access_channels(websocketpp::log::alevel::frame_header);

    // Initialize Asio Transport
    m_server.init_asio();
    m_server.set_reuse_addr(true);

    // Register handler callbacks
    m_server.set_open_handler(bind(&Websocket_server::on_open, this, ::_1));
    m_server.set_close_handler(bind(&Websocket_server::on_close, this, ::_1));
    m_server.set_message_handler(bind(&Websocket_server::on_message, this, ::_1, ::_2));
    m_server.set_fail_handler(bind(&Websocket_server::on_fail, this, ::_1));
    m_server.set_http_handler(bind(&Websocket_server::on_http, this, ::_1));
  }

  void run(uint16_t port) {
    // listen on specified port
    m_server.listen(port);

    // Start the server accept loop
    m_server.start_accept();

    // Start the ASIO io_service run loop
    try {
      LOG_INFO("Websocket server running on port {}\n", port);
      m_server.run();
    } catch (websocketpp::exception const& e) {
      LOG_ERR("Websocket server exception: {}\n", e.what());
    } catch (const std::exception& e) {
      LOG_ERR("Websocket server exception: {}\n", e.what());
    } catch (...) {
      LOG_ERR("Websocket server: other exception\n");
    }
  }

  void stop() {
    {
      lock_guard<mutex> guard(m_action_lock);
      connection_hdl hdl;
      std::queue<action>().swap(m_actions); // clean queue
      m_actions.push(action(STOP, hdl));
    }
    m_action_cond.notify_one();
  }

  void broadcast_message(const std::string& message) {
    if (!m_server.is_listening() && (m_connections.size() == 0)) {
      return;
    }

    {
      lock_guard<mutex> guard(m_action_lock);
      connection_hdl hdl;
      m_actions.push(action(BROADCAST, hdl, message));
    }
    m_action_cond.notify_one();
  }

  void on_open(connection_hdl hdl) {
    {
      lock_guard<mutex> guard(m_action_lock);
      m_actions.push(action(SUBSCRIBE, hdl));
      LOG_INFO("Websocket server on_open\n");
    }
    m_action_cond.notify_one();
  }

  void on_close(connection_hdl hdl) {
    {
      lock_guard<mutex> guard(m_action_lock);
      m_actions.push(action(UNSUBSCRIBE, hdl));
      LOG_INFO("Websocket server on_close\n");
    }
    m_action_cond.notify_one();
  }

  void on_message(connection_hdl hdl, websocketserver::message_ptr msg) {
    // queue message up for sending by processing thread
    {
      lock_guard<mutex> guard(m_action_lock);
      m_actions.push(action(MESSAGE, hdl, msg));
      if (msg->get_opcode() == websocketpp::frame::opcode::text) {
        LOG_INFO("Websocket server on_message: {}\n", msg->get_payload());
      } else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
        LOG_INFO("Websocket server on_message: hex({})\n", msg->get_payload().size());
      }
    }
    m_action_cond.notify_one();
  }

  void on_fail(connection_hdl hdl) {
    websocketserver::connection_ptr con = m_server.get_con_from_hdl(hdl);
    if (con->get_ec() != asio::error::operation_aborted) {
      LOG_ERR("Websocket server failed: {} {}\n", con->get_ec().value(), con->get_ec().message());
    }
  }

  void on_http(connection_hdl hdl) {
    auto con = m_server.get_con_from_hdl(hdl);
    std::string resource = con->get_resource();

    if (resource == "/") {
      std::string body = get_log_page_html();
      con->append_header("Content-Type", "text/html");
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

  void close_all_clients() {
    for (auto& hdl : m_connections) {
      try {
        // initiate the WebSocket closing handshake for these connections
        m_server.close(hdl, websocketpp::close::status::going_away, "Server shuting down");
      } catch (websocketpp::exception const& e) {
        LOG_ERR("Websocket server exception: {}\n", e.what());
      }
    }
  }

  bool is_listening() {
    return m_server.is_listening();
  }

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
      } else if (a.type == UNSUBSCRIBE) {
        lock_guard<mutex> guard(m_connection_lock);
        m_connections.erase(a.hdl);
      } else if (a.type == MESSAGE) {
        lock_guard<mutex> guard(m_connection_lock);
        m_server.send(a.hdl, a.msg); // echo
      } else if (a.type == STOP) {
        lock_guard<mutex> guard(m_connection_lock);
        try {
          // initiate the closing of the server listening socket
          m_server.stop_listening();
          close_all_clients();
          is_running = false;
        } catch (websocketpp::exception const& e) {
          LOG_ERR("Websocket server exception: {}\n", e.what());
          m_connection_lock.unlock();
        }
      } else if (a.type == BROADCAST) {
        lock_guard<mutex> guard(m_connection_lock);
        for (auto& hdl : m_connections) {
          try {
            m_server.send(hdl, a.str, websocketpp::frame::opcode::text);
          } catch (websocketpp::exception const& e) {
            LOG_ERR("Websocket server exception: {}\n", e.what());
          }
        }
      } else {
        // undefined.
      }
    }
  }

 private:
  typedef std::set<connection_hdl, std::owner_less<connection_hdl> > con_list;

  websocketserver m_server;
  con_list m_connections;
  std::queue<action> m_actions;

  mutex m_action_lock;
  mutex m_connection_lock;
  condition_variable m_action_cond;
};

#endif  // !_WEBSOCKET_SERVER_HH_
