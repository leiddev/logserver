/*
 * Leid 2024.3
 */
#include "bl/mutex.h"
#include "bl/time.h"

#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace bl {
Mutex::Mutex() {
#ifdef _WIN32
  CRITICAL_SECTION *crit = new CRITICAL_SECTION();
  memset(crit, 0, sizeof(CRITICAL_SECTION));
  InitializeCriticalSectionEx(crit, 0, 0);
  _lock = crit;

  CONDITION_VARIABLE *cond = new CONDITION_VARIABLE();
  memset(cond, 0, sizeof(CONDITION_VARIABLE));
  InitializeConditionVariable(cond);
  _cond = cond;
  _attr = nullptr;
#else
  _attr = new pthread_mutexattr_t();
  _lock = new pthread_mutex_t();
  _cond = new pthread_cond_t();
  pthread_mutexattr_init((pthread_mutexattr_t *)_attr);
  pthread_mutexattr_settype((pthread_mutexattr_t *)_attr,
                            PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init((pthread_mutex_t *)_lock, (pthread_mutexattr_t *)_attr);
  pthread_cond_init((pthread_cond_t *)_cond, NULL);
#endif
}

Mutex::~Mutex() {
#ifdef _WIN32
  if (_lock != nullptr) {
    DeleteCriticalSection((CRITICAL_SECTION *)_lock);
    delete ((CRITICAL_SECTION *)_lock);
    _lock = nullptr;
  }
  delete ((CONDITION_VARIABLE *)_cond);
  _cond = nullptr;
#else
  pthread_mutexattr_destroy((pthread_mutexattr_t *)_attr);
  pthread_mutex_destroy((pthread_mutex_t *)_lock);
  pthread_cond_destroy((pthread_cond_t *)_cond);
  delete (pthread_mutexattr_t*)_attr;
  delete (pthread_mutex_t*)_lock;
  delete (pthread_cond_t *)_cond;
#endif
}

int Mutex::Lock() {
#ifdef _WIN32
  EnterCriticalSection((CRITICAL_SECTION *)_lock);
  return 0;
#else
  return pthread_mutex_lock((pthread_mutex_t *)_lock);
#endif
}

int Mutex::Unlock() {
#ifdef _WIN32
  LeaveCriticalSection((CRITICAL_SECTION *)_lock);
  return 0;
#else
  return pthread_mutex_unlock((pthread_mutex_t *)_lock);
#endif
}

int Mutex::TryLock(int timeout) {
#ifdef _WIN32
  BOOL success = TryEnterCriticalSection((CRITICAL_SECTION *)_lock);
  if (success) {
    return 0;
  }

  return -1;
#else
  return pthread_mutex_trylock((pthread_mutex_t *)_lock);
#endif
}

int Mutex::CondWait(int timeout) {
#ifdef _WIN32
  EnterCriticalSection((CRITICAL_SECTION *)_lock);
  BOOL success = SleepConditionVariableCS((CONDITION_VARIABLE *)_cond, (CRITICAL_SECTION *)_lock, timeout);
  if (success) {
    return 0;
  }

  return -1;
#else
  struct timespec abstime;
  struct timeval now;

  pthread_mutex_lock((pthread_mutex_t *)_lock);

  bl::gettimeofday(&now, NULL);
  int nsec = now.tv_usec * 1000 + (timeout % 1000) * 1000000;
  abstime.tv_nsec = nsec % 1000000000;
  abstime.tv_sec = now.tv_sec + nsec / 1000000000 + timeout / 1000;

  return pthread_cond_timedwait((pthread_cond_t *)_cond,
                                (pthread_mutex_t *)_lock, &abstime);
#endif
}

int Mutex::CondWait() {
#ifdef _WIN32
  EnterCriticalSection((CRITICAL_SECTION *)_lock);
  BOOL success = SleepConditionVariableCS((CONDITION_VARIABLE *)_cond, (CRITICAL_SECTION *)_lock, INFINITE);
  if (success) {
    return 0;
  }

  return -1;
#else
  pthread_mutex_lock((pthread_mutex_t *)_lock);
  return pthread_cond_wait((pthread_cond_t *)_cond, (pthread_mutex_t *)_lock);
#endif
}

int Mutex::CondSingal() {
#ifdef _WIN32
  WakeConditionVariable((CONDITION_VARIABLE *)_cond);
  return 0;
#else
  return pthread_cond_signal((pthread_cond_t *)_cond);
#endif
}
}  // namespace bl
