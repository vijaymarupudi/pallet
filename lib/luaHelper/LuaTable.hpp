#pragma once

#include "LuaTraits.hpp"
#include "types.hpp"


namespace pallet::luaHelper {
struct LuaTable {
  lua_State* L;
  StackIndex index;
  template <class K, class V>
  inline void rawset(K&& key, V&& value) {
    if constexpr (requires { lua_Integer{key}; })  {
      luaHelper::push(L, value);
      lua_rawseti(L, index.getIndex(), lua_Integer{key});
    } else {
      luaHelper::push(L, key);
      luaHelper::push(L, value);
      lua_rawset(L, index.getIndex());
    }
  }

  static inline LuaTable create(lua_State* L) {
    lua_newtable(L);
    return LuaTable{L, lua_gettop(L)};
  }

  static inline LuaTable from(lua_State* L, RegistryIndex reg) {
    luaHelper::push(L, reg);
    return LuaTable{L, lua_gettop(L)};
  }

  static inline LuaTable from(lua_State* L, void* key) {
    registryPush(L, key);
    return LuaTable{L, lua_gettop(L)};
  }

  operator StackIndex () const {
    return index;
  }
};


// template <class Custom>
// struct Properties {
//   static inline pull(lua_State* L, ) 
// }

template <>
struct LuaTraits<LuaTable> {
  static inline void push(lua_State* L, auto&& table) {
    luaHelper::push(L, table.index);
  }

  static inline bool check(lua_State* L, int index) {
    return lua_istable(L, index);
  }

  static inline LuaTable pull(lua_State* L, int index) {
    return LuaTable{L, index};
  }
};

}
