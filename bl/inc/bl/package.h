/*
 * lujy 2020.6
 */
#ifndef _BL_PACKAGE_H_
#define _BL_PACKAGE_H_

#include "exp.h"
#include <memory>
#include <sstream>
#include <string>

namespace bl {

class BL_API Object {
 public:
  virtual std::string Name() {
    return "";
  }
};

class BL_API Package {
 public:
  virtual std::string ToString() {
    return "";
  }
};

using sptrPackage = std::shared_ptr<bl::Package>;

class BL_API Buffer {
 public:
  Buffer() {
    addr = nullptr;
    len = 0;
    owner = nullptr;
    carrier = nullptr;
  }

  Buffer(void *p, long long l, void *o = 0, void *c = 0) {
    addr = p;
    len = l;
    owner = o;
    carrier = c;
  }

  ~Buffer() {
  }

 public:
  void *addr;
  long long len;
  void *owner;
  void *carrier;
};

}  // namespace bl

#endif  // !_BL_PACKAGE_H_
