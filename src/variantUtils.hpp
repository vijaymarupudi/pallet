#pragma once

#include <variant>

namespace pallet {

namespace detail {
template <class V>
struct VariantForEach {};

template <class... Types, size_t... indicies>
void variantForEachApplication(auto&& lambda, std::index_sequence<indicies...>) {
  (lambda.template operator()<Types>(indicies), ...);
}

template <class... Types>
struct VariantForEach<std::variant<Types...>> {
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
