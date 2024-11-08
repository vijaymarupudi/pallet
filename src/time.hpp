#pragma once

#include <concepts>
#include <cinttypes>
#include "macros.hpp"

namespace pallet {

  namespace detail {
    template <class T>
    concept TimeArgument = std::convertible_to<T, uint64_t>;
  }

  PALLET_ALWAYS_INLINE uint64_t timeInS(detail::TimeArgument auto s) {
    return s * 1000 * 1000;
  }

  PALLET_ALWAYS_INLINE uint64_t timeInMs(detail::TimeArgument auto ms) {
    return ms * 1000;
  }

  PALLET_ALWAYS_INLINE uint64_t timeInUs(detail::TimeArgument auto us) {
    return us;
  }
}
