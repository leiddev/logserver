/*
 * Leid 2024.3
 */
#include "bl/thread.h"
#include "bl/time.h"
#include "common.h"

#ifdef _WIN32
#define _WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif

#ifdef _WIN32
typedef struct {
  HANDLE handle;
  void *(*start_routine)(void *);
  void *arg;
  DWORD tid;
  void *exit_code;
} pthread_t;

static unsigned __stdcall win32_start_routine(void *arg) {
  pthread_t *thread = (pthread_t *)arg;
  thread->tid = GetCurrentThreadId();
  thread->exit_code = thread->start_routine(thread->arg);
  return 0;
}

static int pthread_create(pthread_t *thread, const void *unused, void *(*start_routine)(void *), void *arg) {
  thread->arg = arg;
  thread->start_routine = start_routine;
  thread->handle = (HANDLE)_beginthreadex(NULL, 0, win32_start_routine, thread, 0, NULL);
  if (!thread->handle) {
    return errno;
  } else {
    return 0;
  }
}

static int win32_pthread_join(pthread_t *thread, void **value_ptr) {
  DWORD result = WaitForSingleObject(thread->handle, INFINITE);
  switch (result) {
    case WAIT_OBJECT_0:
      if (value_ptr)
        *value_ptr = thread->exit_code;
      CloseHandle(thread->handle);
      return 0;
    case WAIT_ABANDONED:
      CloseHandle(thread->handle);
      return EINVAL;
    default:
      /* the wait failed, so do not detach */
      DWORD err = GetLastError();
      return -1;
  }
}
#endif

namespace bl {
Thread::Thread() {
  _tid = new pthread_t();
  is_running_ = false;
}

Thread::~Thread() {
  delete (pthread_t *)_tid;
}

int Thread::SetName(const std::string &name) {
#ifdef _WIN32
  // unfortunately this is only available on Windows 10
  return 0;
#else
  int err = 0;
  _name = name.substr(0,15);
  //LOG_INFO("{}", _name.c_str());
  err = pthread_setname_np(*(pthread_t *)_tid, _name.c_str());
  CHECK_ERR(err);

  return err;
#endif
}

std::string Thread::GetName() {
  return _name;
}

void *Thread::Proxy(void *param) {
  Thread *pt = (Thread *)param;
  pt->Run();
  return NULL;
}

int Thread::Start(void *(*entrance)(void *), void *param) {
  int err = 0;
  CHECK_PTR(entrance);
  is_running_ = true;
  err = pthread_create((pthread_t *)_tid, NULL, entrance, param);
  CHECK_ERR(err);
  return err;
}

int Thread::Start() {
  int err = 0;
  if (is_running_)
    return 0;
  is_running_ = true;
  err = pthread_create((pthread_t *)_tid, NULL, Proxy, this);
  CHECK_ERR(err);
  return err;
}

int Thread::Run() {
  while (is_running_) {
    Sleep(1000);
  }
  return 0;
}

int Thread::Stop() {
  if (!is_running_)
    return 0;
  is_running_ = false;
  void *status = 0;
#ifdef _WIN32
  win32_pthread_join((pthread_t *)_tid, &status);
#else
  pthread_join(*(pthread_t *)_tid, &status);
#endif
  return 0;
}

void Thread::Sleep(int ms) {
  return sleep(ms);
}
}  // namespace bl
