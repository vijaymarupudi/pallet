#pragma once

#include <atomic>
#include <inttypes.h>
#include <array>
#include <optional>

template <class T, size_t _capacity>
class ThreadSafeRingBuffer {
  using index_type = size_t;
private:
  T storage[_capacity];
  
  std::atomic<index_type> readIndex;
  std::atomic<index_type> writeIndex;
  
  index_type increment(index_type n) {
    return (n + 1) & (this->capacity() - 1);
  }

public:
  ThreadSafeRingBuffer(): readIndex(0),
                          writeIndex(0) {}

  template <class Type>
  bool push(Type&& item) {
    index_type currentWriteIndex = writeIndex;
    index_type nextWriteIndex = this->increment(currentWriteIndex);
    if (nextWriteIndex == readIndex) {
      return false;
    }
    storage[currentWriteIndex] = std::forward<Type>(item);
    writeIndex = nextWriteIndex;
    return true;
  }

  std::optional<T> read() {
    index_type currentReadIndex = readIndex;
    if (currentReadIndex == writeIndex) {
      return std::optional<T>{};
    }
    readIndex = this->increment(currentReadIndex);
    T ret = std::move(this->storage[currentReadIndex]);
    return ret;
  }
  
  index_type capacity() {
    return _capacity;
  }
  
};
