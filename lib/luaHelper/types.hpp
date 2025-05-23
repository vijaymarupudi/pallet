#pragma once

#include "LuaTraits.hpp"

namespace pallet::luaHelper {
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
    lua_rawgeti(L, LUA_REGISTRYINDEX, index.index);
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


}
