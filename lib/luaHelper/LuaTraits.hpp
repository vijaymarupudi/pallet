#pragma once

#include <tuple>
#include <type_traits>
#include <concepts>


#include "lua.hpp"
#include "pallet/functional.hpp"
#include "concepts.hpp"

namespace pallet::luaHelper {


template <class ValueType>
struct LuaTraits {};

namespace concepts {
template <class T>
concept Pushable = requires (lua_State* L, T value) {
  LuaTraits<std::remove_cvref_t<T>>::push(L, value);
};

template <class T>
concept Returnable = std::same_as<T, void> || Pushable<T>;


template <class T>
concept GeneralCheckable = requires (lua_State* L, int index) {
  { LuaTraits<std::remove_cvref_t<T>>::check(L, index) } -> std::same_as<bool>;
};

template <class T>
concept EfficientCheckable = requires (lua_State* L, int index, int type) {
  { LuaTraits<std::remove_cvref_t<T>>::check(L, index, type) } -> std::same_as<bool>;
};

template <class T>
concept Checkable = GeneralCheckable<T> && EfficientCheckable<T>;

template <class T, class V>
concept NotSameAs = !std::same_as<T, V>;

template <class T>
concept Pullable = !std::is_rvalue_reference_v<T> && requires (lua_State* L, int index) {
  // the return type is allowed to be different from the input type (LuaNumber support)
  { LuaTraits<T>::pull(L, index) } -> NotSameAs<void>;
};

}

static inline void push(lua_State* L, auto&& value) {
  static_assert(concepts::Pushable<decltype(value)>);
  LuaTraits<std::remove_cvref_t<decltype(value)>>::push(L, std::forward<decltype(value)>(value));
}

template <class T>
static inline decltype(auto) pull(lua_State* L, int index) {
  static_assert(!std::is_rvalue_reference_v<T>, "Cannot pull rvalue reference");
  static_assert(concepts::Pullable<T>, "Value needs to be pullable");
  return LuaTraits<T>::pull(L, index);
}

template <class T>
static inline bool isType(lua_State* L, int index) {
  if constexpr (concepts::GeneralCheckable<T>) {
    return LuaTraits<std::remove_cvref_t<T>>::check(L, index);  
  } else if constexpr (concepts::EfficientCheckable<T>) {
    auto type = lua_type(L, index);
    return LuaTraits<std::remove_cvref_t<T>>::check(L, index, type);
  } else {
    static_assert(false, "Cannot check type");
  }
}

template <class T>
static inline bool isType(lua_State* L, int index, int type) {
  if constexpr (concepts::GeneralCheckable<T>) {
    return LuaTraits<std::remove_cvref_t<T>>::check(L, index);  
  } else if constexpr (concepts::EfficientCheckable<T>) {
    return LuaTraits<std::remove_cvref_t<T>>::check(L, index, type);
  } else {
    static_assert(false, "Cannot check type");
  }
}


template <class T>
static inline bool isTypeEfficient(lua_State* L, int index, int type) {  
  if constexpr (concepts::EfficientCheckable<T>) {
    return LuaTraits<std::remove_cvref_t<T>>::check(L, index, type);
  } else {
    static_assert(false, "Cannot check type efficient");
  }
}

template <class K, class V>
static inline void rawSetTable(lua_State* L, int tableIndex, K&& key, V&& value) {
  push(L, std::forward<K>(key));
  push(L, std::forward<V>(value));
  lua_rawset(L, tableIndex);
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

template <class... Types>
auto
checkedPullMultiple(lua_State* L, int baseIndex = 1) {
  return ([&]<int... indexes>(std::integer_sequence<int, indexes...>) {
      return std::make_tuple(checkedPull<Types>(L, baseIndex + indexes)...);
    })(std::make_integer_sequence<int, sizeof...(Types)>{});
}

template <>
struct LuaTraits<bool> {
  static inline bool check(lua_State* L, int index, int type) {
    (void)L;
    (void)index;
    return type == LUA_TBOOLEAN;
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
  static inline bool check(lua_State* L, int index, int type) {
    (void)L;
    (void)index;
    return type == LUA_TSTRING;
  }

  static inline void push(lua_State* L, const std::string_view& str) {
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
  static inline bool check(lua_State* L, int index, int type) {
    return type == LUA_TNUMBER && !lua_isinteger(L, index);
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
  static inline bool check(lua_State* L, int index, int type) {
    (void)L;
    (void)index;
    return type == LUA_TFUNCTION;
  }

  static inline void push(lua_State* L, T val) {
    lua_pushcfunction(L, val);
  }
};


template <>
struct LuaTraits<const char*> {
  static inline void push(lua_State* L, const char* str) {
    lua_pushstring(L, str);
  }
};


// lua function reference (function without converting to pointer)
template <>
struct LuaTraits<int(lua_State* L)> {
private:
  // using T = lua_CFunction;
  public:
  static inline bool check(lua_State* L, int index) {
    return lua_iscfunction(L, index);
  }

  static inline void push(lua_State* L, auto&& val) {
    lua_pushcfunction(L, val);
  }
};

template <class T>
requires std::is_pointer_v<T>
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index, int type) {
    (void)L;
    (void)index;
    return type == LUA_TUSERDATA;
  }

  static inline void push(lua_State* L, T val) {
    lua_pushlightuserdata(L, val);
  }

  static inline T pull(lua_State* L, int index) {
    return static_cast<T>(lua_touserdata(L, index));
  }
};

}
