#pragma once

#include <utility>
#include "lua.hpp"
#include "LuaTable.hpp"


namespace pallet::luaHelper {

namespace detail {
inline char finalizableMetatableLocation = 'f';

}

static inline void createFinalizableMetatable(lua_State* L) {
  auto table = LuaTable::create(L);
  table.rawset("__gc", +[](lua_State* L) {
    void* memory = lua_touserdata(L, -1);
    lua_getiuservalue(L, -1, 1);
    auto cleanupFunction = reinterpret_cast<void(*)(void*)>(lua_touserdata(L, -1));
    if (cleanupFunction) {
      cleanupFunction(memory);
    }
    lua_pop(L, 2); // cleanupFunction, userdata
    return 0;
  });
  lua_rawsetp(L, LUA_REGISTRYINDEX, &detail::finalizableMetatableLocation);
}

static inline void pullFinalizableMetatable(lua_State* L) {
  lua_rawgetp(L, LUA_REGISTRYINDEX, &detail::finalizableMetatableLocation);
}

template <class T, class... Args>
static inline T& createFinalizableUserData(lua_State* L, Args&&... args) {
  void* memory = lua_newuserdatauv(L, sizeof(T), 1);
  pullFinalizableMetatable(L);
  lua_setmetatable(L, -2);
  if (std::is_trivially_destructible_v<T>) {
    luaHelper::push(L, static_cast<void*>(nullptr));
  } else {
    luaHelper::push(L, reinterpret_cast<void*>(+[](T* value) {
      value->~T();
    }));  
  }
  lua_setiuservalue(L, -2, 1);
  return *std::construct_at(static_cast<T*>(memory), std::forward<Args>(args)...);
}
}
