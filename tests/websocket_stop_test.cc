// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 *
 * Reproducer for the shutdown hang in Websocket_server::process_messages().
 *
 * The STOP branch holds m_connection_lock with a lock_guard and, when
 * m_server.stop_listening() throws, calls m_connection_lock.unlock() inside
 * the catch block. The lock_guard unlocks the mutex a second time when it goes
 * out of scope, and is_running is never set to false, so the processing thread
 * keeps waiting for another action and never joins.
 *
 * stop_listening() throws websocketpp::error::invalid_state when the endpoint
 * never started listening, which happens when run() failed to bind the web
 * port (for example because another process holds it) and the signal handler
 * still calls stop(). The test calls stop() on a server that never ran and
 * waits for the processing thread to leave.
 *
 * The expected behaviour is that process_messages() returns. On the current
 * code the wait expires and the test reports the hang. It then calls exit(1)
 * instead of returning, because the processing thread cannot be joined and
 * the std::thread destructor would call std::terminate.
 */

#include "websocket_server.hh"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

int main() {
  applog::Logger::instance().setLevel(applog::kCritical);

  Websocket_server server;
  std::atomic<bool> exited(false);
  std::thread processor([&server, &exited] {
    server.process_messages();
    exited.store(true, std::memory_order_release);
  });

  // Let the processing thread reach its wait loop, then ask it to stop the
  // same way a signal handler would when run() never started listening.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  server.stop();

  for (int i = 0; i < 30 && !exited.load(std::memory_order_acquire); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (!exited.load(std::memory_order_acquire)) {
    std::fprintf(stderr,
                 "FAIL process_messages() did not exit after stop() on a server that never listened; "
                 "the STOP handler leaves is_running set and unlocks m_connection_lock twice\n");
    std::exit(1);
  }

  processor.join();
  std::printf("process_messages() exited after stop()\n");
  return 0;
}
