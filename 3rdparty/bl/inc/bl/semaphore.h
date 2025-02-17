#ifndef _BL_SEMAPHORE_H_
#define _BL_SEMAPHORE_H_

#include "exp.h"

//#ifdef __cplusplus
// extern "C" {
//#endif

namespace bl {
class BL_API Semaphore {
 public:
  Semaphore();
  ~Semaphore();

 public:
  int Wait();
  int Post();

 private:
  void* _sem;
};
}  // namespace bl

//#ifdef __cplusplus
//}
//#endif

#endif
