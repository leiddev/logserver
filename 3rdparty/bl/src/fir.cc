/*
 * lujy 2020.6
 */
#include "bl/fir.h"

using std::queue;

namespace bl {
template <class T, int n>
FIR<T, n>::FIR() {
  _sum = 0;
  _bypass = false;
}

template <class T, int n>
FIR<T, n>::~FIR() {
}

template <class T, int n>
void FIR<T, n>::SetByPass(bool bypass) {
  _bypass = bypass;
}

template <class T, int n>
T FIR<T, n>::f(T x) {
  if (_bypass)
    return x;

  _queue.push(x);
  if (_queue.size() <= n) {
    _sum += x;
    return _sum / _queue.size();
  } else {
    T old = _queue.front();
    _queue.pop();
    _sum -= old;
    _sum += x;
  }
  return _sum / n;
}

}  // namespace bl