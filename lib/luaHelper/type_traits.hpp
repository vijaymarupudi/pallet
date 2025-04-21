#pragma once

namespace pallet::luaHelper {
template <auto data>
struct constant_wrapper {
  using value_type = decltype(data);
  static constexpr value_type value = data;
  constexpr operator value_type() const {
    return data;
  }
};


template <auto value>
constexpr constant_wrapper<value> cw{};
}
