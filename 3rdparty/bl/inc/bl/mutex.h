#ifndef _BL_MUTEX_H_
#define _BL_MUTEX_H_

#include "exp.h"

//#ifdef __cplusplus
// extern "C" {
//#endif

namespace bl {
class BL_API Mutex {
 public:
  Mutex();
  ~Mutex();

 public:
  int Lock();
  int Unlock();
  int TryLock(int timeout);
  int CondWait(int timeout);
  int CondWait();
  int CondSingal();

 private:
  void* _lock;
  void* _cond;
  void* _attr;
};
}  // namespace bl

//#ifdef __cplusplus
//}
//#endif

#endif
