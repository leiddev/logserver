/*
 * leid 2024.3
 */
#ifndef _BL_MSGQ_H_
#define _BL_MSGQ_H_

#include "exp.h"
#include <deque>
#include <chrono>
#include <mutex>
#include <condition_variable>

namespace bl {

template <class T> class BL_API MsgQ {
 public:
  MsgQ(int max_size = -1) {
    this->max_size = max_size;
  }

  ~MsgQ() {
    queue.clear();
  }

  int Add(T msg) {
    std::unique_lock<std::mutex> locker(mtx);
    if ((max_size == -1) || (queue.size() < max_size)) {
      queue.emplace_back(msg);
      if (queue.size() == 1) {
        cond.notify_one();
      }
      return queue.size();
    } else {
      return -1;
    }
  }

  int AddFront(T msg) {
    std::unique_lock<std::mutex> locker(mtx);
    if ((max_size == -1) || (queue.size() < max_size)) {
      queue.emplace_front(msg);
      if (queue.size() == 1) {
        cond.notify_one();
      }
      return queue.size();
    } else {
      return -1;
    }
  }

  T Get() {
    std::unique_lock<std::mutex> locker(mtx);
    cond.wait(locker, [&]() { return !queue.empty(); });
    T msg = queue.front();
    queue.pop_front();
    return msg;
  }

  int Wait(int timeout_ms) {
    std::chrono::milliseconds timeout(timeout_ms);
    std::unique_lock<std::mutex> locker(mtx);

    if (!cond.wait_for(locker, timeout, [&]() { return !queue.empty(); })) { /* timeout */
      return -1;
    }
    return 0;
  }


 private:
  std::deque<T> queue;
  std::mutex mtx;
  std::condition_variable cond;
  int max_size = -1;
};
}  // namespace bl


#endif  /* _BL_MSGQ_H_ */
