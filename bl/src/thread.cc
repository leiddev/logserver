/*
* lujy 2021.6
*/
#include "bl/thread.h"
#include "bl/time.h"
#include "common.h"
#include "pthread.h"

namespace bl {
Thread::Thread() {
  _tid = new pthread_t();
  is_running_ = false;
}

Thread::~Thread() {
  delete (pthread_t *)_tid;
}

int Thread::SetName(const std::string &name) {
  int err = 0;
  _name = name.substr(0,15);
  //LOG_INFO("{}", _name.c_str());
  err = pthread_setname_np(*(pthread_t *)_tid, _name.c_str());
  CHECK_ERR(err);

  return err;
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
  pthread_join(*(pthread_t *)_tid, &status);
  return 0;
}

void Thread::Sleep(int ms) {
  return sleep(ms);
}
}  // namespace bl
