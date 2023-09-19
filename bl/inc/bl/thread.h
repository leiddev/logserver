/*
* lujy 2021.6
*/
#ifndef _BL_THREAD_H_
#define _BL_THREAD_H_

#include "exp.h"

#include <string>

//#ifdef __cplusplus
// extern "C" {
//#endif

namespace bl {
class BL_API Thread {
 public:
  Thread();
  virtual ~Thread();

 public:
  static void *Proxy(void *param);
  int Start(void *(*entrance)(void *), void *param);
  int Start();
  int SetName(const std::string &name);
  std::string GetName();
  virtual int Run();
  int Stop();
  bool GetThreadStatus(void) { return is_running_; };
  void SetThreadStatus(bool status) { is_running_ = status; };
  void Sleep(int ms);

 private:
  void *_tid;
  bool is_running_;
  std::string _name;
};
}  // namespace bl
//#ifdef __cplusplus
//}
//#endif

#endif
