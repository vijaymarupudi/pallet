#pragma once

#include <cinttypes>
#include "macros.hpp"

namespace pallet {
  PALLET_ALWAYS_INLINE uint64_t timeInS(uint64_t s) {
    return s * 1000 * 1000;
  }

  PALLET_ALWAYS_INLINE uint64_t timeInS(double s) {
    return s * 1000 * 1000;
  }

  PALLET_ALWAYS_INLINE uint64_t timeInMs(uint64_t ms) {
    return ms * 1000;
  }

  PALLET_ALWAYS_INLINE uint64_t timeInMs(double ms) {
    return ms * 1000;
  }

  PALLET_ALWAYS_INLINE uint64_t timeInUs(uint64_t us) {
    return us;
  }

  PALLET_ALWAYS_INLINE uint64_t timeInUs(double us) {
    return us;
  }
}
