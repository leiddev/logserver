/*
 * lujy 2020.6
 */
#include "bl/iir.h"

namespace bl {

template <class T>
IIR<T>::IIR() {
  _y = -1;
  _is_first = true;
  _s = 0.7;
  _bypass = false;
}

template <class T>
IIR<T>::~IIR() {
}

template <class T>
void IIR<T>::SetS(double s) {
  _s = s;
}

template <class T>
void IIR<T>::SetBypass(bool bypass) {
  _bypass = bypass;
}

template <class T>
T IIR<T>::f(T x) {
  if (_is_first || _bypass) {
    _is_first = false;
    _y = x;
    return x;
  }
  _y = _y * _s + x * (1 - _s);
  return (T)_y;
}
}  // namespace bl