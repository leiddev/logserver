// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 *
 * Reproducer for the data race on Websocket_server::m_connections.
 *
 * broadcast_message() reads m_connections.size() from the UDP thread without
 * taking m_connection_lock, while process_messages() inserts the handles of
 * new clients and erases the handles of gone ones under that lock. Two threads
 * touching a std::set that way is undefined behaviour; ThreadSanitizer shows
 * it as a race between the size() read and _M_insert_unique()/_M_erase().
 *
 * This binary must be built with ThreadSanitizer:
 *
 *   cmake -S . -B build -DWEBSERVER_TYPE=WEBSOCKET -DLOG_SERVER_TSAN_TESTS=ON
 *   cmake --build build --target websocket_broadcast_race_test
 *   ctest --test-dir build -R websocket_broadcast_race --output-on-failure
 *
 * On the current code the report points at websocket_server.hh (the
 * m_connections.size() read) and the process exits with status 66.
 */
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define LOGSERVER_THREAD_SANITIZER 1
#endif
#endif
#if defined(__SANITIZE_THREAD__)
#define LOGSERVER_THREAD_SANITIZER 1
#endif
#ifndef LOGSERVER_THREAD_SANITIZER
#error "websocket_broadcast_race_test must be built with -fsanitize=thread"
#endif

#include "websocket_server.hh"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {
constexpr int kBroadcastThreads = 4;
constexpr int kBroadcastsPerThread = 20000;
constexpr int kConnectCycles = 20000;
}  // namespace

int main() {
  // The race reports are the output we want; keep the application logging out
  // of the way so the run stays readable.
  applog::Logger::instance().setLevel(applog::kCritical);

  Websocket_server server;
  std::thread processor([&server] { server.process_messages(); });

  std::atomic<bool> go(false);
  std::vector<std::thread> broadcasters;
  broadcasters.reserve(kBroadcastThreads);
  for (int i = 0; i < kBroadcastThreads; ++i) {
    broadcasters.emplace_back([&server, &go] {
      while (!go.load(std::memory_order_acquire)) {
      }
      for (int n = 0; n < kBroadcastsPerThread; ++n) {
        server.broadcast_message("race line\n");
      }
    });
  }

  go.store(true, std::memory_order_release);

  // on_open()/on_close() only queue actions; process_messages() does the
  // actual insert and erase on m_connections.
  websocketpp::connection_hdl handle;
  for (int i = 0; i < kConnectCycles; ++i) {
    server.on_open(handle);
    server.on_close(handle);
  }

  for (auto& broadcaster : broadcasters) {
    broadcaster.join();
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::printf("broadcast/subscribe window complete, exiting with the sanitizer verdict\n");
  // Deliberately do not call stop(): the processing thread is still waiting for
  // an action, and that shutdown path has its own reproducer. Exiting here
  // leaves the ThreadSanitizer exit status as the only result of this binary.
  std::exit(0);
}
