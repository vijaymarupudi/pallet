#pragma once

#include <string_view>
#include <tuple>
#include <cstdlib>
#include <utility>
#include <concepts>

#include "lua.hpp"
#include "luaHelper/luaTraits.hpp"

namespace pallet::luaHelper {

// template <class ValueType>
// struct LuaTraits;


template <class K, class V>
static inline void rawSetTable(lua_State* L, int tableIndex, K&& key, V&& value) {
  push(L, std::forward<K>(key));
  push(L, std::forward<V>(value));
  lua_rawset(L, tableIndex);
}

static inline void push(lua_State* L, auto&& value) {
  LuaTraits<std::remove_cvref_t<decltype(value)>>::push(L, std::forward<decltype(value)>(value));
}

template <class T>
static inline bool isType(lua_State* L, int index) {
  return LuaTraits<std::remove_reference_t<T>>::check(L, index);
}

template <class T>
static inline decltype(auto) pull(lua_State* L, int index) {
  if constexpr (std::is_lvalue_reference_v<T>) {
    return LuaTraits<T>::pull(L, index);
  } else {
    // rvalue or value type
    return LuaTraits<std::remove_reference_t<T>>::pull(L, index);
  }
  
}

template <class T>
decltype(auto) checkedPull(lua_State* L, int index) {
  if (isType<T>(L, index)) [[likely]] {
    return pull<T>(L, index);
  } else {
    luaL_argerror(L, index, "wrong argument");
    std::unreachable();
  }
}

namespace detail {
template <class... Types, int... indexes>
inline std::tuple<Types...> _luaCheckedPullMultiple(lua_State* L,
                                                    int baseIndex,
                                                    std::integer_sequence<int, indexes...>) {
  return std::make_tuple(checkedPull<Types>(L, baseIndex + indexes)...);
}
}

template <class... Types>
std::tuple<Types...>
checkedPullMultiple(lua_State* L, int baseIndex) {
  return detail::_luaCheckedPullMultiple<Types...>(L, baseIndex, std::make_integer_sequence<int, sizeof...(Types)>{});
}


}
