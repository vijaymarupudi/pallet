#pragma once

#include "LuaTraits.hpp"

namespace pallet::luaHelper {
struct StackIndex {
  int index;
};

struct RegistryIndex {
  int index;
};

template <>
struct LuaTraits<StackIndex> {
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

}
