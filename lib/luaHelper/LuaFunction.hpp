#pragma once

#include "../luaHelper.hpp"

namespace pallet::luaHelper {
template <class FunctionType>
struct LuaFunction;

template <class ReturnType, class... Args>
struct LuaFunction<ReturnType(Args...)> {
  lua_State* L;
  int ref;

  LuaFunction(LuaFunction&& other) : L(other.L), ref(std::exchange(other.ref, -1)) {}
  LuaFunction& operator=(LuaFunction&& other) = default;
  
  ReturnType operator()(Args... args) {
    lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
    (luaHelper::push(args), ...);
    lua_call(L, sizeof...(Args), 1);
    auto&& ret = luaHelper::pull<ReturnType>(L, -1);
    lua_pop(L, 1);
    return std::forward<decltype(ret)>(ret);
  }

  ~LuaFunction() {
    if (ref >= 0) {
      luaL_unref(L, LUA_REGISTRYINDEX, ref);
    }
  }
};

template <class R, class... A>
struct LuaTraits<LuaFunction<R, A...>> {
  static inline bool check(lua_State* L, int index) {
    return lua_isfunction(L, index);
  }

  static inline LuaFunction<R, A...> pull(lua_State* L, int index) {
    lua_pushvalue(L, index);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);
    return LuaFunction<R, A...>{L, ref};
  }
};

}

