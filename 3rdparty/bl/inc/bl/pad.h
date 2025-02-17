/*
 * lujy 2021.5
 */
#ifndef _PAD_H_
#define _PAD_H_

#include "bl/json.h"
#include "bl/mutex.h"
#include "bl/package.h"
#include "bl/thread.h"
#include "bl/time.h"
#include "exp.h"
#include <queue>

namespace bl {

enum class pad_type { PAD_TYPE_SRC = 0x0, PAD_TYPE_SINK = 0x1 };
enum class pad_mode { PAD_MODE_PUSH = 0x0, PAD_MODE_PULL = 0x1 };

using sptrPackageQueue = std::queue<sptrPackage>;

class Pad {
 public:
  Pad(pad_type type = pad_type::PAD_TYPE_SRC, size_t max_queue_size = 1,
      pad_mode mode = pad_mode::PAD_MODE_PUSH);
  ~Pad();

  void PushData(sptrPackage);
  sptrPackage PopData();
  sptrPackage PopData(int timeout);
  void ClearDataQueue();

  size_t GetQueueSize() { return _in_queue._data.size(); }
  size_t GetQueueMaxSize() { return _in_queue.max_size; }

  void SetNextPad(Pad *pad) { _next = pad; }
  Pad *GetNextPad() { return _next; }
  void SetPrevPad(Pad *pad) { _prev = pad; }
  Pad *GetPrevPad() { return _prev; }

 void AddSrcFrameNum() {
    frame_cnt_mutex.Lock();
    _frame_cnt ++;
    frame_cnt_mutex.Unlock();
  }

  size_t GetSrcFrameNum() {
    frame_cnt_mutex.Lock();
    size_t f = _frame_cnt;
    frame_cnt_mutex.Unlock();
    return f;
  }

  void ResetSrcFrameNum() {
    frame_cnt_mutex.Lock();
    _frame_cnt = 0;
    frame_cnt_mutex.Unlock();
  }

  class DataQueue {
   public:
    DataQueue() { max_size = 1; }
    bl::Mutex _mutex;
    sptrPackageQueue _data;
    size_t max_size;
  };

 protected:
  DataQueue _in_queue;
  Pad *_next;
  Pad *_prev;
  pad_type _type;
  pad_mode _mode;

 private:
  bl::Mutex frame_cnt_mutex;
  size_t _frame_cnt = 0;
};

}  // namespace bl
#endif
