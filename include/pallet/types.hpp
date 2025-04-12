#pragma once
#include <cstddef>
#include <type_traits>

namespace pallet {
struct Blank {};

template <size_t i, class... Args>
using IndexVariadic = __type_pack_element<i, Args...>;

template <typename T, typename... Ts>
struct IndexOfTypeInVariadicStruct;

template <typename T, typename... Ts>
struct IndexOfTypeInVariadicStruct<T, T, Ts...> : std::integral_constant<std::size_t, 0> {};

template <typename T, typename U, typename... Ts>
struct IndexOfTypeInVariadicStruct<T, U, Ts...> : std::integral_constant<std::size_t, 1 + IndexOfTypeInVariadicStruct<T, Ts...>::value> {};

template <class T, class... Types>
constexpr size_t IndexOfTypeInVariadic = IndexOfTypeInVariadicStruct<T, Types...>::value;


namespace types::detail {
constexpr size_t max(size_t x, size_t y) {
  if (x > y) return x;
  return y;
}
}

constexpr size_t maxOfSizeTs(auto&&... vals) {
  size_t maxSize = 0;
  ((maxSize = types::detail::max(maxSize, vals)), ...);
  return maxSize;
}

}
