/*
 * lujy 2021.6
 */
#include "bl/mutex.h"
#include "bl/time.h"
#include "pthread.h"

namespace bl {
Mutex::Mutex() {
  _attr = new pthread_mutexattr_t();
  _lock = new pthread_mutex_t();
  _cond = new pthread_cond_t();
  pthread_mutexattr_init((pthread_mutexattr_t *)_attr);
  pthread_mutexattr_settype((pthread_mutexattr_t *)_attr,
                            PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init((pthread_mutex_t *)_lock, (pthread_mutexattr_t *)_attr);
  pthread_cond_init((pthread_cond_t *)_cond, NULL);
}

Mutex::~Mutex() {
  pthread_mutexattr_destroy((pthread_mutexattr_t *)_attr);
  pthread_mutex_destroy((pthread_mutex_t *)_lock);
  pthread_cond_destroy((pthread_cond_t *)_cond);
  delete (pthread_mutexattr_t*)_attr;
  delete (pthread_mutex_t*)_lock;
  delete (pthread_cond_t *)_cond;
}

int Mutex::Lock() {
  return pthread_mutex_lock((pthread_mutex_t *)_lock);
}

int Mutex::Unlock() {
  return pthread_mutex_unlock((pthread_mutex_t *)_lock);
}

int Mutex::TryLock(int timeout) {
  return 0;
}

int Mutex::CondWait(int timeout) {
  struct timespec abstime;
  struct timeval now;

  pthread_mutex_lock((pthread_mutex_t *)_lock);

  bl::gettimeofday(&now, NULL);
  int nsec = now.tv_usec * 1000 + (timeout % 1000) * 1000000;
  abstime.tv_nsec = nsec % 1000000000;
  abstime.tv_sec = now.tv_sec + nsec / 1000000000 + timeout / 1000;

  return pthread_cond_timedwait((pthread_cond_t *)_cond,
                                (pthread_mutex_t *)_lock, &abstime);
}

int Mutex::CondWait() {
  pthread_mutex_lock((pthread_mutex_t *)_lock);
  return pthread_cond_wait((pthread_cond_t *)_cond, (pthread_mutex_t *)_lock);
}

int Mutex::CondSingal() {
  return pthread_cond_signal((pthread_cond_t *)_cond);
}
}  // namespace bl
