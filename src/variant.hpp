#pragma once

#include <variant>
#include <type_traits>

namespace pallet {

template <class... Types>
using Variant = std::variant<Types...>;

  template <class T>
  struct isVariant : std::false_type {};

  template <class... Types>
  struct isVariant<Variant<Types...>> : std::true_type {};

  template <class T>
  concept VariantConcept = isVariant<T>::value;

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

}
