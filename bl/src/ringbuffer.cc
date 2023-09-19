#include "bl/ringbuffer.h"
#include "common.h"
#include <string>
#include <cstring>

using std::memcpy;

namespace bl {
RingBuffer::RingBuffer(size_t capacity) {
  _data = new char[capacity];  // head != tail
  _capacity = capacity;
  _head = _data;
  _tail = _data;
  _is_empty = true;
}

// copy constructor
RingBuffer::RingBuffer(const RingBuffer& other) {
  _capacity = other._capacity;
  _data = new char[_capacity];
  memcpy(_data, other._data, _capacity);
  _head = _data + (other._head - other._data);
  _tail = _data + (other._tail - other._data);
  _is_empty = other._is_empty;
}

// move constructor
RingBuffer::RingBuffer(RingBuffer&& other) {
  _capacity = other._capacity;
  _data = other._data;
  _head = other._head;
  _tail = other._tail;
  _is_empty = other._is_empty;

  other._capacity = 0;
  other._data = nullptr;
  other._head = nullptr;
  other._tail = nullptr;
  other._is_empty = true;
}

RingBuffer::~RingBuffer() {
  delete[] _data;
  _capacity = 0;
  _head = _tail = _data = 0;
}

size_t RingBuffer::Put(const char *ptr, size_t size) {
  if (!ptr || size == 0)
    return 0;

  if (_capacity <= size) {
    memcpy(_data, ptr + (size - _capacity), _capacity);
    _head = _data;
    _tail = _head + _capacity;
    _is_empty = false;
    return _capacity;
  }

  bool is_overwrite = false;

  if (size > RemainSize()) {
    is_overwrite = true;
  }

  int tail2boundry = (_data + _capacity) - _tail;
  int cut_size = size - tail2boundry;

  if (cut_size < 0) {  // <=?
    memcpy(_tail, ptr, size);
    _tail += size;
  } else {
    memcpy(_tail, ptr, tail2boundry);
    memcpy(_data, ptr + tail2boundry, cut_size);
    _tail = _data + cut_size;
  }

  if (is_overwrite) {
    _head = _tail;
  }
  _is_empty = false;
  return size;
}

size_t RingBuffer::Get(char *ptr, size_t size) {
  if (!ptr)
    return -1;

  size = size <= Size() ? size : Size();

  int head2boundry = (_data + _capacity) - _head;
  int cut_size = size - head2boundry;

  if (cut_size < 0) {
    memcpy(ptr, _head, size);
    _head += size;
  } else {
    memcpy(ptr, _head, head2boundry);
    memcpy(ptr + head2boundry, _data, cut_size);
    _head = _data + cut_size;
  }

  if (_tail == _head) {
    _is_empty = true;
  }
  return size;
}

char &RingBuffer::operator[](const size_t index) const{
  size_t i = index % _capacity;
  if (_head + i < _data + _capacity) {
    return _head[i];
  }
  return _data[i - ((_data + _capacity) - _head)];
}

// assignment operator
RingBuffer& RingBuffer::operator= (const RingBuffer& other) {
  if (this != &other) {
    _capacity = other._capacity;
    _data = new char[_capacity];
    memcpy(_data, other._data, _capacity);
    _head = _data + (other._head - other._data);
    _tail = _data + (other._tail - other._data);
    _is_empty = other._is_empty;
  }
  return *this;
}

size_t RingBuffer::Size() const {
  if (_is_empty) {
    return 0;
  }
  return (_tail > _head) ? _tail - _head : _capacity - (_head - _tail);
}

size_t RingBuffer::RemainSize() const {
  return _capacity - Size();
}

size_t RingBuffer::Capacity() const{
  return _capacity;
}

void RingBuffer::Clear() {
  _head = _tail = _data;
  _is_empty = true;
}

void RingBuffer::Dump() const {
  for (size_t i = 0; i < this->Size(); i++) {
    printf("%c", (*this)[i]);
  }
  printf("\n");
}

static void _printf_buffer(const RingBuffer &buffer) {
  for (size_t i = 0; i < buffer.Size(); i++) {
    printf("%d, ", buffer[i]);
  }
  printf("\n");
}

void RingBufferTest() {
  RingBuffer buffer(10);
  char input[7] = {1, 2, 3, 4, 5, 6, 7};
  char output[10] = {0};

  buffer.Put(input, 5);
  _printf_buffer(buffer);

  buffer.Put(input, 4);
  _printf_buffer(buffer);

  buffer.Get(output, 5);
  _printf_buffer(buffer);

  buffer.Put(input, 3);
  _printf_buffer(buffer);

  buffer.Get(output, 2);
  _printf_buffer(buffer);

  buffer.Put(input, 7);
  _printf_buffer(buffer);

  buffer.Get(output, 3);
  buffer[0] = 123;
  _printf_buffer(buffer);

}

}  // namespace bl