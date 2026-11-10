// SPDX-License-Identifier: MIT
/*
 * Leid 2026.11
 *
 * Regression test for the shutdown of Websocket_server::process_messages().
 *
 * The STOP branch used to hold m_connection_lock with a lock_guard and call
 * m_connection_lock.unlock() from its catch block when m_server.stop_listening()
 * threw. The lock_guard then unlocked the mutex a second time, and is_running
 * stayed set, so the processing thread kept waiting for another action and the
 * join in main() hung.
 *
 * stop_listening() throws websocketpp::error::invalid_state when the endpoint
 * never started listening, which happens when run() failed to bind the web
 * port (for example because another process holds it) and the signal handler
 * still calls stop(). The test calls stop() on a server that never ran, twice,
 * and waits for the processing thread to leave.
 *
 * The thread cannot be joined if the hang is back, so the test calls exit(1)
 * in that case instead of returning: the std::thread destructor would
 * otherwise call std::terminate.
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
  // same way a signal handler would when run() never started listening. A
  // second call covers the destructor running after the signal handler.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));
  server.stop();
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
