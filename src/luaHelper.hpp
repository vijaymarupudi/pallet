#pragma once

#include <string_view>
#include <tuple>

#include "lua.hpp"

namespace pallet::luaHelper {

namespace detail {

template <class T>
concept Boolean = std::is_same_v<T, bool>;

template <class T>
concept Integer = std::is_integral_v<T>;

template <class T>
concept FloatingPoint = std::is_floating_point_v<T>;
}

using namespace detail;

void luaPush(lua_State* L, const std::string_view str) {
  lua_pushlstring(L, str.data(), str.size());
}

void luaPush(lua_State* L, void* ptr) {
  lua_pushlightuserdata(L, ptr);
}

void luaPush(lua_State* L, const lua_CFunction func) {
  lua_pushcfunction(L, func);
}

void luaPush(lua_State* L, const Integer auto integer) {
  lua_pushinteger(L, static_cast<lua_Integer>(integer));
}

void luaPush(lua_State* L, const FloatingPoint auto number) {
  lua_pushnumber(L, static_cast<lua_Number>(number));
}



template <class T>
bool luaIsType(lua_State* L, int index) {
  if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
    return lua_isinteger(L, index);
  } else if constexpr (std::is_floating_point_v<T>) {
    return lua_isnumber(L, index);
  } else if constexpr (std::is_pointer_v<T>) {
    return lua_islightuserdata(L, index);
  } else if constexpr (std::is_same_v<T, std::string_view>) {
    return lua_isstring(L, index);
  }
  else {
    static_assert(false);
  }
}



template <class T>
T luaPull(lua_State* L, int index) {
  if constexpr (std::is_integral_v<T> || std::is_enum_v<T>) {
    auto ret = lua_tointeger(L, index);
    return static_cast<T>(ret);
  } else if constexpr (std::is_floating_point_v<T>) {
    auto ret = lua_tonumber(L, index);
    return static_cast<T>(ret);
  } else if constexpr (std::is_pointer_v<T>) {
    auto ret = lua_touserdata(L, index);
    return static_cast<T>(ret);
  } else if constexpr (std::is_same_v<T, std::string_view>) {
    size_t len;
    auto str = lua_tolstring(L, index, &len);
    return std::string_view(str, len);
  } else {
    static_assert(false);
  }
}


template <class T>
T luaCheckedPull(lua_State* L, int index) {
  if (luaIsType<T>(L, index)) [[likely]] {
    return luaPull<T>(L, index);
  } else {
    luaL_argerror(L, index, "wrong argument");
    return T{};
  }
}


namespace detail {
template <class... Types, int... indexes>
inline std::tuple<Types...> _luaCheckedPullMultiple(lua_State* L,
                                                    int baseIndex,
                                                    std::integer_sequence<int, indexes...>) {
  return std::make_tuple(luaCheckedPull<Types>(L, baseIndex + indexes)...);
}
}


template <class... Types>
std::tuple<Types...>
luaCheckedPullMultiple(lua_State* L, int baseIndex = 1) {
  return _luaCheckedPullMultiple<Types...>(L, baseIndex, std::make_integer_sequence<int, sizeof...(Types)>{});
}


void luaRawSetTable(lua_State* L, int tableIndex, const auto& key, const auto& value) {
  luaPush(L, key);
  luaPush(L, value);
  lua_rawset(L, tableIndex);
}

}
