#pragma once

#include "lua.hpp"
#include "luaHelper.hpp"

namespace pallet {
static inline int getRegistryEntry(lua_State* L, const void* key) {
  lua_rawgetp(L, LUA_REGISTRYINDEX, key);
  return 1;
}

static inline int getRegistryEntry(lua_State* L, int index) {
  lua_rawgeti(L, LUA_REGISTRYINDEX, index);
  return 1;
}

static inline int getPalletCTable(lua_State* L) {
  return getRegistryEntry(L, &_palletCTableRegistryIndex);
}

static inline LuaInterface& getLuaInterfaceObject(lua_State* L) {
  getRegistryEntry(L, &_luaInterfaceRegistryIndex);
  auto ret = luaHelper::pull<LuaInterface*>(L, -1);
  lua_pop(L, 1);
  return *ret;
} 
}
