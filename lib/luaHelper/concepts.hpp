#pragma once

#include <type_traits>

namespace pallet::luaHelper::concepts {

namespace detail {
template <class T>
struct StatelessIntLikeHelper : T {
  int val;
};
}

template <class T>
concept Stateless = sizeof(detail::StatelessIntLikeHelper<T>) == sizeof(int);

template <class T>
concept HasCallOperator = requires {
  &T::operator();
};

template <class T>
concept StatelessLambdaLike = Stateless<std::remove_reference_t<T>> && HasCallOperator<std::remove_reference_t<T>>;


template <class T, class V>
concept DecaysTo = std::is_same_v<std::decay_t<T>, V>;

}
