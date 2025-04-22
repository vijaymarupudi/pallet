#pragma once

#include <concepts>
#include "LuaTraits.hpp"

namespace pallet::luaHelper {

struct ReturnStackTop {};

struct StackIndex {
  int index;
  int getIndex() const {
    return index;
  }
};

struct RegistryIndex {
  int index;
  int getIndex() const {
    return index;
  }
  
  operator bool () const {
    return !(index == LUA_NOREF);
  }

  RegistryIndex& operator=(const std::nullptr_t&) {
    index = LUA_NOREF;
    return *this;
  }

};

template <>
struct LuaTraits<StackIndex> {
  static inline bool check(lua_State* L, int index) {
    (void)L;
    (void)index;
    return true;
  }
  static inline void push(lua_State* L, StackIndex index) {
    lua_pushvalue(L, index.index);
  }

  static inline StackIndex pull(lua_State* L, int index) {
    (void)L;
    return StackIndex{index};
  }
};


template <>
struct LuaTraits<RegistryIndex> {
  static inline void push(lua_State* L, RegistryIndex index) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, index.getIndex());
  }
};

template <>
struct LuaTraits<ReturnStackTop> {
  static inline void push(lua_State* L, ReturnStackTop) {
    (void)L;
  }
};

static inline RegistryIndex store(lua_State* L, auto&& item) {
  luaHelper::push(L, item);
  return RegistryIndex{luaL_ref(L, LUA_REGISTRYINDEX)};
};

static inline void free(lua_State* L, RegistryIndex index) {
  luaL_unref(L, LUA_REGISTRYINDEX, index.getIndex());
}

template <class T>
static inline T pull(lua_State* L, RegistryIndex index) {
  luaHelper::push(L, index);
  auto&& val = luaHelper::pull<T>(L, -1);
  lua_pop(L, 1);
  return std::forward<decltype(val)>(val);
}

template <class T>
static inline T pull(lua_State* L, StackIndex index) {
  return luaHelper::pull<T>(L, index.getIndex());
}


struct LuaNumber {
  lua_Number number;
  inline constexpr LuaNumber(lua_Number n) : number(n) {};
};

template <>
struct LuaTraits<LuaNumber> {
  static inline bool check(lua_State* L, int index) {
    return lua_type(L, index) == LUA_TNUMBER;
  }

  static inline void push(lua_State* L, auto&& val) {
    lua_pushnumber(L, val.number);
  }

  static inline lua_Number pull(lua_State* L, int index) {
    return lua_tonumber(L, index);
  }
};

}
