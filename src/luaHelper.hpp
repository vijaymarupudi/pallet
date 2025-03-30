#pragma once

#include <string_view>
#include <tuple>
#include <cstdlib>
#include <utility>

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

inline void luaPush(lua_State* L, const std::string_view str) {
  lua_pushlstring(L, str.data(), str.size());
}

inline void luaPush(lua_State* L, void* ptr) {
  lua_pushlightuserdata(L, ptr);
}

inline void luaPush(lua_State* L, const lua_CFunction func) {
  lua_pushcfunction(L, func);
}

inline void luaPush(lua_State* L, const Integer auto integer) {
  lua_pushinteger(L, static_cast<lua_Integer>(integer));
}

inline void luaPush(lua_State* L, const FloatingPoint auto number) {
  lua_pushnumber(L, static_cast<lua_Number>(number));
}

inline void luaPush(lua_State* L, const VariantConcept auto item) {
  std::visit([&](auto&& x) {
    luaPush(L, x);
  }, item);
}

namespace detail {
  template <class T>
  concept enumeration = std::is_enum_v<T>;
  template <class T>
  concept pointer = std::is_pointer_v<T>;
}

template <class T>
bool luaIsType(lua_State* L, int index) {
  static_assert(false, "Don't know how to check this lua type");
  return false;
}

template <>
inline bool luaIsType<bool>(lua_State* L, int index) {
  return lua_isboolean(L, index);
}

template <std::integral T>
bool luaIsType(lua_State* L, int index) {
  return lua_isinteger(L, index);
}

template <enumeration T>
bool luaIsType(lua_State* L, int index) {
  return lua_isinteger(L, index);
}

template <std::floating_point T>
bool luaIsType(lua_State* L, int index) {
  return lua_isnumber(L, index);
}


template <detail::pointer T>
bool luaIsType(lua_State* L, int index) {
  return lua_islightuserdata(L, index);
}

template <>
inline bool luaIsType<std::string_view>(lua_State* L, int index) {
  return lua_isstring(L, index);
}

template <class T>
struct LuaIsTypeVariant;

template <class... Types>
struct LuaIsTypeVariant<Variant<Types...>> {
  static bool apply(lua_State* L, int index) {
    return (luaIsType<Types>(L, index) || ...);
  }
};

template <VariantConcept T>
bool luaIsType(lua_State* L, int index) {
  return LuaIsTypeVariant<T>::apply(L, index);
}

template <class T>
T luaPull(lua_State* L, int index);

template <>
inline bool luaPull<bool>(lua_State* L, int index) {
  return lua_toboolean(L, index);
}

template <>
inline std::string_view luaPull<std::string_view>(lua_State* L, int index) {
  size_t len = 0;
  auto str = lua_tolstring(L, index, &len);
  return std::string_view(str, len);
}

template <class T>
T luaPull(lua_State* L, int index) requires (std::integral<T> || detail::enumeration<T>) {
  auto ret = lua_tointeger(L, index);
  return static_cast<T>(ret);
}

template <std::floating_point T>
T luaPull(lua_State* L, int index) {
    auto ret = lua_tonumber(L, index);
    return static_cast<T>(ret);
}

template <detail::pointer T>
T luaPull(lua_State* L, int index) {
  auto ret = lua_touserdata(L, index);
  return static_cast<T>(ret);
}

  template <class T>
  struct luaPullVariant;

  template <class... Types>
  struct luaPullVariant<Variant<Types...>> {
    using variant_type = Variant<Types...>;

    template <class First, class... ArgTypes>
    static variant_type recursive_apply_many(lua_State* L, int index) {
      if (luaIsType<First>(L, index)) {
          return luaPull<First>(L, index);
      } else {
        if constexpr (sizeof...(ArgTypes) == 0) {
          luaL_argerror(L, index, "wrong argument");
          std::unreachable();
        } else {
          return recursive_apply_many<ArgTypes...>(L, index);
        }
      }
    }
    
    static variant_type apply(lua_State* L, int index) {
      return recursive_apply_many<Types...>(L, index);
    }
  };

template <VariantConcept T>
T luaPull(lua_State* L, int index) {
  return luaPullVariant<T>::apply(L, index);
}

template <class T>
T luaCheckedPull(lua_State* L, int index) {
  if (luaIsType<T>(L, index)) [[likely]] {
    return luaPull<T>(L, index);
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
  luaPush(L, key);
  luaPush(L, value);
  lua_rawset(L, tableIndex);
}

}
