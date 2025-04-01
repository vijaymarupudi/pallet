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
#include "pallet/memory.hpp"
#include "pallet/error.hpp"


namespace pallet {

  namespace detail {
    struct PipeProperties {
      void operator()(std::array<int, 2>& pipes) noexcept {
        if (!(pipes[0] < 0)) {
          close(pipes[0]);
        }

        if (!(pipes[1] < 0)) {
          close(pipes[1]);
        }
      }
      
      bool isValid(const std::array<int, 2>& pipes) const {
        return pipes[0] >= 0;
      }
      void setValid(std::array<int, 2>& pipes, bool val) {
        if (!val) {
          pipes[0] = -1;
          pipes[1] = -1;
        }
      }
    };
  }

  struct Pipe : private UniqueResource<std::array<int, 2>, detail::PipeProperties> {
  public:
    static Result<Pipe> create() {
      int pipes[2];
      pipe(pipes);
      return Pipe(pipes[0], pipes[1]);
    }

    int getReadFd() const {
      return object[0];
    }

    int getWriteFd() const {
      return object[1];
    }

    void write(void* buffer, size_t size) {
      ::write(getWriteFd(), buffer, size);
    }

    // Pipe(Pipe&& other) : UniqueResource(std::move(other)) {};
    // Pipe& operator=(Pipe&& other) {
    //   UniqueResource::operator=(std::move(other));
    //   return *this;
    // }
  private:
    Pipe(int rfd, int wfd) : UniqueResource(std::array<int, 2>{rfd, wfd}) {}
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
