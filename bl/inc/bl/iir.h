/*
 * lujy 2019.6
 */

#ifndef _BL_IIR_H_
#define _BL_IIR_H_

#include "exp.h"

namespace bl {
template <class T>
class BL_API IIR {
 public:
  IIR();
  ~IIR();

 public:
  void SetS(double);
  void SetBypass(bool);
  T CALL f(T x);

 private:
  double _y;
  double _s;
  bool _bypass;
  bool _is_first;
};
}  // namespace bl
#endif