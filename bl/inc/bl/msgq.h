/*
 * leid 2022.8
 */
#ifndef _BL_MSGQ_H_
#define _BL_MSGQ_H_

#include "exp.h"
#include "bl/time.h"
#include <deque>
#include <pthread.h>
#include <cerrno>
#include <cstdio>

namespace bl {

template <class T> class BL_API MsgQ {
 public:
  MsgQ(int max_size=-1) {
    pthread_condattr_init(&condattr);
    pthread_condattr_setclock(&condattr, CLOCK_MONOTONIC);
    pthread_mutex_init(&mtx, nullptr);
    pthread_cond_init(&cond, &condattr);
    this->max_size = max_size;
  }

  ~MsgQ() {
    queue.clear();
    pthread_mutex_destroy(&mtx);
    pthread_cond_destroy(&cond);
    pthread_condattr_destroy(&condattr);
  }

  void Add(T msg) {
    pthread_mutex_lock(&mtx);
    if ((max_size == -1) || (queue.size() < max_size)) {
      queue.emplace_back(msg);
      if (queue.size() == 1) {
        pthread_cond_signal(&cond);
      }
    }
    pthread_mutex_unlock(&mtx);
  }

  void AddFront(T msg) {
    pthread_mutex_lock(&mtx);
    if ((max_size == -1) || (queue.size() < max_size)) {
      queue.emplace_front(msg);
      if (queue.size() == 1) {
        pthread_cond_signal(&cond);
      }
    }
    pthread_mutex_unlock(&mtx);
  }

 T Get() {
    pthread_mutex_lock(&mtx);
    while (queue.empty()) {
      pthread_cond_wait(&cond, &mtx);
    }
    T msg = queue.front();
    queue.pop_front();
    pthread_mutex_unlock(&mtx);
    return msg;
  }

  int Wait(int timeout_ms) {
    struct timespec start_ts;
    struct timespec end_ts;

    clock_gettime(CLOCK_MONOTONIC, &start_ts);
    end_ts = ns_to_ts(ts_to_ns(start_ts) + (long long)timeout_ms * 1000000);

    pthread_mutex_lock(&mtx);
    while (queue.empty()) {
      if (pthread_cond_timedwait(&cond, &mtx, &end_ts) == ETIMEDOUT) { /* timeout */
          pthread_mutex_unlock(&mtx);
          return -1;
      }
    };
    pthread_mutex_unlock(&mtx);
    return 0;
  }

 private:
  std::deque<T> queue;
  pthread_mutex_t mtx;
  pthread_cond_t cond;
  pthread_condattr_t condattr;
  int max_size = -1;
};
}  // namespace bl


#endif  /* _BL_MSGQ_H_ */
