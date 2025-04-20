#pragma once

#include <type_traits>

namespace pallet::concepts {

template <class T, class V>
concept DecaysTo = std::is_same_v<std::decay_t<T>, V>;

}
