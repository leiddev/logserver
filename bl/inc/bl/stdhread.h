/*
 * lujy 2022.11
 */
#ifndef _BL_STD_THREAD_H_
#define _BL_STD_THREAD_H_

#include "exp.h"

#include <functional>
#include <thread>

namespace bl {
class STDThread {
 public:
  STDThread() {
  }
  ~STDThread() {
  }

 public:
  int Start(std::function<void(bool &)> func) {
    _is_running = true;
    _handle = std::make_shared<std::thread>(func, std::ref(_is_running));
    //_handle->detach();
    return 0;
  }

  int Stop() {
    _is_running = false;
    if (_handle->joinable())
      _handle->join();
    return 0;
  }
  bool GetRunningStatus() {
    return _is_running;
  }

 private:
  bool _is_running = false;
  std::shared_ptr<std::thread> _handle;
};
}  // namespace bl
#endif