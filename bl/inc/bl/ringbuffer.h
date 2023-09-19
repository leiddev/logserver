#ifndef _BL_RINGBUFFER_H_
#define _BL_RINGBUFFER_H_

#include <cstddef>

namespace bl {
class RingBuffer {
 public:
  explicit RingBuffer(size_t);
  RingBuffer(const RingBuffer&);  // copy constructor
  RingBuffer(RingBuffer&&);       // move constructor
  ~RingBuffer();

 public:
  size_t Put(const char *, size_t);
  size_t Get(char *, size_t);
  size_t Size() const;
  size_t RemainSize() const;
  size_t Capacity() const;
  void Dump() const;
  void Clear();
  char &operator[](size_t) const;
  RingBuffer& operator= (const RingBuffer& other); // assignment operator

 private:
  size_t _capacity;
  char *_head;
  char *_tail;
  char *_data;
  bool _is_empty;
};
void RingBufferTest();
}  // namespace bl

#endif