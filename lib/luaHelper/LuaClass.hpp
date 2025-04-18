#pragma once

#include "../luaHelper.hpp"

namespace pallet::luaHelper {
template <class T>
class LuaClass {
  lua_State* L;
  int metatableRef;
  const char* name;
public:

  LuaClass(lua_State* L, const char* name) : L(L), name(name) {

    // Create metatable
    lua_newtable(L);
    lua_pushvalue(L, -1);
    metatableRef = luaL_ref(L, LUA_REGISTRYINDEX);
    auto metatable = lua_gettop(L);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      pallet::luaHelper::rawSetTable(L, metatable, "__gc", [](T* ptr) {
        ptr->~T();
      });
    }
    luaHelper::push(L, "__index");
    lua_pushvalue(L, metatable);
    lua_rawset(L, metatable);
  }

  LuaClass(LuaClass&& other) : L(other.L),
                               metatableRef(std::exchange(other.metatableRef, LUA_NOREF)) {}

  template <class... Args>
  T& pushObject(Args&&... args) {
    void* storage = lua_newuserdatauv(L, sizeof(T), 0);
    lua_rawgeti(L, LUA_REGISTRYINDEX, metatableRef);
    lua_setmetatable(L, -2);
    auto ptr = ::new (storage) T (std::forward<Args>(args)...);
    return *ptr;
  }

  template <auto memberFunc>
  void addMethodBatch(int index, const char* name) {
    (pallet::overloads {
      [&]<class R, class... A>(R(T::*)(A...)) {
        pallet::luaHelper::rawSetTable(L, index, name, [](T* ptr, A... args) {
          return (ptr->*memberFunc)(std::move(args)...);
        });
      },
        [&]<class R, class... A>(R(T::*)(A...) const) {
          pallet::luaHelper::rawSetTable(L, index, name, [](T* ptr, A... args) {
            return (ptr->*memberFunc)(std::move(args)...);
          });
        }        
    })(memberFunc);
  }

  int beginBatch() {
    lua_rawgeti(L, LUA_REGISTRYINDEX, metatableRef);
    auto top = lua_gettop(L);
    return top;
  }

  void endBatch(int index) {
    lua_settop(L, index - 1);
  }

  template <auto memberFunc>
  void addMethod(const char* name) {
    auto b = beginBatch();
    addMethodBatch<memberFunc>(b, name);
    endBatch(b);
  }

  ~LuaClass() {
    if (metatableRef != LUA_NOREF) {
      luaL_unref(L, LUA_REGISTRYINDEX, metatableRef);
    }
  }
};

}
