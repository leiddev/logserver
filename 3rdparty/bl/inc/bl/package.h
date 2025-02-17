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
    _addr = nullptr;
    _size = 0;
    _valid_size = 0;
  }
  Buffer(size_t size) {
    _size = size;
    _valid_size = size;
    _addr = new unsigned char[_size];
    // printf("%s new buffer :0x%p, %ld\n", __FUNCTION__, _addr, _size);
  }
  ~Buffer() {
    if (_addr) {
      // printf("%s delete buffer :0x%p, %ld\n", __FUNCTION__, _addr, _size);
      delete[] _addr;
      _addr = nullptr;
    }
    _size = 0;
  }

  int Alloc(size_t size) {
    _size = size;
    _valid_size = size;
    _addr = new unsigned char[_size];
    if (!_addr)
      return -1;
    return 0;
  }

  void SetValidSize(size_t size) {
    _valid_size = size;
  }

  size_t GetValidSize() const {
    return _valid_size;
  }

  unsigned char *GetAddr() const {
    return _addr;
  }

  size_t GetSize() const {
    return _size;
  }
  unsigned char &operator[](size_t idx) const {
    return _addr[idx];
  }

 protected:
  unsigned char *_addr;
  size_t _size;
  size_t _valid_size;
};

using sptrBuffer = std::shared_ptr<Buffer>;
}  // namespace bl

#endif  // !_BL_PACKAGE_H_
