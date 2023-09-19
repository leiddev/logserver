/*
 * lujy 2019.6
 */
#ifndef _BL_FIR_H_
#define _BL_FIR_H_

#include <queue>
#include "exp.h"

namespace bl {
template <class T, int n>
class BL_API FIR {
 public:
  FIR();
  ~FIR();

 public:
  void SetByPass(bool);

  T f(T);

 private:
  std::queue<T> _queue;
  T _sum;
  bool _bypass;
};
}  // namespace bl
#endif