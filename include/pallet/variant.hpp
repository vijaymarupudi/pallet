#pragma once

#include <cstddef>
#include <variant>
#include <type_traits>
#include "pallet/overloaded.hpp"

namespace pallet {

template <class... Types>
using Variant = std::variant<Types...>;

template <class V>
inline constexpr size_t variantSize = std::variant_size_v<V>;

  template <class T>
  struct isVariant : std::false_type {};

  template <class... Types>
  struct isVariant<Variant<Types...>> : std::true_type {};

  namespace concepts {
    template <class T>
    concept Variant = isVariant<T>::value;
  }


namespace detail {
template <class V>
struct VariantForEach {};

template <class... Types, size_t... indicies>
void variantForEachApplication(auto&& lambda, std::index_sequence<indicies...>) {
  (lambda.template operator()<Types, indicies>(), ...);
}

template <class... Types>
struct VariantForEach<Variant<Types...>> {
  static void apply(auto&& lambda) {
    variantForEachApplication<Types...>(std::forward<decltype(lambda)>(lambda),
                                 std::index_sequence_for<Types...>{});
  }
};
}

template <class V>
void variantForEach(auto&& lambda) {
  detail::VariantForEach<V>::apply(std::forward<decltype(lambda)>(lambda));
}

namespace detail {
template <class V>
concept VariantLike = concepts::Variant<std::remove_cvref_t<V>>;
}

// Index of the active alternative
template <detail::VariantLike V>
constexpr size_t variantIndex(const V& variant) {
  return variant.index();
}

// Checked access by alternative index or type, like std::get
template <size_t I>
constexpr decltype(auto) get(detail::VariantLike auto&& variant) {
  return std::get<I>(std::forward<decltype(variant)>(variant));
}

template <class Type>
constexpr decltype(auto) get(detail::VariantLike auto&& variant) {
  return std::get<Type>(std::forward<decltype(variant)>(variant));
}

// Pointer to the alternative if active, otherwise nullptr. Takes a
// reference, unlike std::get_if.
template <size_t I, detail::VariantLike V>
constexpr auto get_if(V& variant) {
  return std::get_if<I>(&variant);
}

template <class Type, detail::VariantLike V>
constexpr auto get_if(V& variant) {
  return std::get_if<Type>(&variant);
}

// A pointer or reference into a const temporary would dangle
template <size_t I, detail::VariantLike V>
void get_if(const V&&) = delete;

template <class Type, detail::VariantLike V>
void get_if(const V&&) = delete;

template <class Type>
constexpr decltype(auto) get_unchecked(detail::VariantLike auto& variant) {
  return *pallet::get_if<Type>(variant);
}

template <class Type>
void get_unchecked(const detail::VariantLike auto&&) = delete;

template <class... Args>
decltype(auto) visit(Args&&... args) {
  return std::visit(std::forward<Args>(args)...);
}


}
