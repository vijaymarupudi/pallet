#pragma once

#include <string_view>
#include <tuple>
#include <cstdlib>
#include <utility>
#include <concepts>

#include "lua.hpp"

#include "pallet/variant.hpp"

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

template <class ValueType>
struct LuaTraits;

static inline void push(lua_State* L, auto&& value) {
  LuaTraits<std::remove_cvref_t<decltype(value)>>::push(L, std::forward<decltype(value)>(value));
}

template <class T>
static inline bool luaIsType(lua_State* L, int index) {
  return LuaTraits<T>::check(L, index);
}

template <class T>
static inline T pull(lua_State* L, int index) {
  return LuaTraits<T>::pull(L, index);
}

template <>
struct LuaTraits<bool> {
  static inline bool check(lua_State* L, int index) {
    return lua_isboolean(L, index);
  }

  static inline void push(lua_State* L, bool val) {
    lua_pushboolean(L, val);
  }

  static inline bool pull(lua_State* L, int index) {
    return lua_toboolean(L, index);
  }
};

template <class T>
requires (std::integral<T> || std::is_enum_v<T>)
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index) {
    return lua_isinteger(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushinteger(L, static_cast<lua_Integer>(val));
  }

  static inline T pull(lua_State* L, int index) {
    auto ret = lua_tointeger(L, index);
    return static_cast<T>(ret);
  }
};

template <class T>
requires (std::is_convertible_v<T, std::string_view>)
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index) {
    return lua_isstring(L, index);
  }

  static inline void push(lua_State* L, std::string_view str) {
    lua_pushlstring(L, str.data(), str.size());
  }

  static inline std::string_view pull(lua_State* L, int index) {
    size_t len = 0;
    auto str = lua_tolstring(L, index, &len);
    return std::string_view(str, len);
  }
};


template <std::floating_point T>
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index) {
    return lua_isnumber(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushnumber(L, static_cast<lua_Number>(val));
  }

  static inline T pull(lua_State* L, int index) {
    auto ret = lua_tonumber(L, index);
    return static_cast<T>(ret);
  }
};
  
template <>
struct LuaTraits<lua_CFunction> {
private:
  using T = lua_CFunction;
  public:
  static inline bool check(lua_State* L, int index) {
    return lua_iscfunction(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushcfunction(L, val);
  }
};

template <std::convertible_to<lua_CFunction> T>
struct LuaTraits<T> {
private:
  // using T = lua_CFunction;
  public:
  static inline bool check(lua_State* L, int index) {
    return lua_iscfunction(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushcfunction(L, val);
  }
};

template <class T>
requires std::is_pointer_v<T>
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index) {
    return lua_islightuserdata(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushlightuserdata(L, val);
  }

  static inline T pull(lua_State* L, int index) {
    return static_cast<T>(lua_touserdata(L, index));
  }
};



template <class T>
T luaCheckedPull(lua_State* L, int index) {
  if (luaIsType<T>(L, index)) [[likely]] {
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
  return std::make_tuple(luaCheckedPull<Types>(L, baseIndex + indexes)...);
}
}


template <class... Types>
std::tuple<Types...>
luaCheckedPullMultiple(lua_State* L, int baseIndex = 1) {
  return _luaCheckedPullMultiple<Types...>(L, baseIndex, std::make_integer_sequence<int, sizeof...(Types)>{});
}


void luaRawSetTable(lua_State* L, int tableIndex, const auto& key, const auto& value) {
  push(L, key);
  push(L, value);
  lua_rawset(L, tableIndex);
}

}
