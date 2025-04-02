#pragma once

#if defined(__x86_64__)
// INTEL
#include <immintrin.h>
#elif defined(__ARM_ACLE)
// ARM
#include <arm_acle.h>
#else
#error "Don't know the current architecture"
#endif

#include <array>
#include <unistd.h>
#include "pallet/error.hpp"


namespace pallet {

struct Pipe {
public:
  static Result<Pipe> create() {
    int fds[2];
    pipe(fds);
    return Pipe(fds[0], fds[1]);
  }

  int fds[2];

  Pipe(int rfd, int wfd) : fds{rfd, wfd} {}

  int getReadFd() const {
    return fds[0];
  }

  int getWriteFd() const {
    return fds[1];
  }

  void write(void* buffer, size_t size) {
    ::write(getWriteFd(), buffer, size);
  }

  Pipe(Pipe&& other) {
    fds[0] = other.fds[0];
    fds[1] = other.fds[1];
    other.fds[0] = -1;
    other.fds[1] = -1;
  }

  Pipe& operator=(Pipe&& other) {
    std::swap(fds[0], other.fds[0]);
    std::swap(fds[1], other.fds[1]);
    return *this;
  }

  ~Pipe() {
    if (fds[0] >= 0) {
      close(fds[0]);
      close(fds[1]);
    }
  }    
};


static inline void architecturePause() {
#if defined(__x86_64__)
  // INTEL
  _mm_pause();
#elif defined(__ARM_ACLE)
  // ARM
  __yield();
#endif
}

}
