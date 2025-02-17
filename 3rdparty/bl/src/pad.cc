/*
 * lujy 2021.6
 */
#include "bl/pad.h"
#include "common.h"

namespace bl {

Pad::Pad(pad_type type, size_t max_queue_size, pad_mode mode) {
  _type = type;
  _mode = mode;
  _next = nullptr;
  _prev = nullptr;
  _in_queue.max_size = max_queue_size;
}

Pad::~Pad() {}
sptrPackage Pad::PopData() {
  _in_queue._mutex.Lock();
  // LOG_DEBUG("{} Element wake up...", this->ToString());
  if (_in_queue._data.empty()) {
    _in_queue._mutex.Unlock();
    return NULL;
  }

  sptrPackage pack = _in_queue._data.front();
  _in_queue._data.pop();
  _in_queue._mutex.Unlock();
  return pack;
}

sptrPackage Pad::PopData(int timeout_ms) {
  _in_queue._mutex.CondWait(timeout_ms);
  if (_in_queue._data.empty()) {
    _in_queue._mutex.Unlock();
    return NULL;
  }
  sptrPackage pack = _in_queue._data.front();
  _in_queue._data.pop();
  _in_queue._mutex.Unlock();
  return pack;
}

void Pad::PushData(sptrPackage pack) {
  if (!pack)
    return;
  AddSrcFrameNum();
  if (_type == pad_type::PAD_TYPE_SRC)
    return _next->PushData(pack);

  int err = 0;
  _in_queue._mutex.Lock();
  if (_in_queue._data.size() <= _in_queue.max_size) {
    _in_queue._data.push(pack);
  }

  err = _in_queue._mutex.CondSingal();
  if (err) {
    LOG_ERR("signal err {}", err);
  }
  _in_queue._mutex.Unlock();
}

void Pad::ClearDataQueue() {
  _in_queue._mutex.Lock();
  while (!_in_queue._data.empty()) {
    _in_queue._data.pop();
  }
  _in_queue._mutex.Unlock();
}

}  // namespace bl