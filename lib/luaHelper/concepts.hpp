#pragma once

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

}
