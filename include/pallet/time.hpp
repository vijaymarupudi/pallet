#pragma once

#include <concepts>
#include <cstdint>
#include "pallet/macros.hpp"

namespace pallet {

  using Time = uint64_t;
  using STime = int64_t;

  namespace detail {
    template <class T>
    concept TimeArgument = std::convertible_to<T, pallet::Time>;
  }

  constexpr PALLET_ALWAYS_INLINE pallet::Time timeInS(detail::TimeArgument auto s) {
    return s * 1000 * 1000 * 1000;
  }

  constexpr PALLET_ALWAYS_INLINE pallet::Time timeInMs(detail::TimeArgument auto ms) {
    return ms * 1000 * 1000;
  }

  constexpr PALLET_ALWAYS_INLINE pallet::Time timeInUs(detail::TimeArgument auto us) {
    return us * 1000;
  }

  constexpr PALLET_ALWAYS_INLINE double timeToS(detail::TimeArgument auto time) {
    return time / 1000.0 / 1000.0 / 1000.0;
  }

  constexpr PALLET_ALWAYS_INLINE double timeToMs(detail::TimeArgument auto time) {
    return time / 1000.0 / 1000.0;
  }

  constexpr PALLET_ALWAYS_INLINE double timeInUs(detail::TimeArgument auto time) {
    return time / 1000.0;
  }

}
