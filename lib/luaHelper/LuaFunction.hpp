#pragma once

#include "../luaHelper.hpp"

namespace pallet::luaHelper {
template <class FunctionType>
struct LuaFunction;

template <class ReturnType, class... Args>
struct LuaFunction<ReturnType(Args...)> {
  lua_State* L;
  RegistryIndex luaFunction;

  LuaFunction(lua_State* L, RegistryIndex luaFunction) : L(L), luaFunction(luaFunction) {}
  LuaFunction(LuaFunction&& other) : L(other.L), luaFunction(std::exchange(other.luaFunction, nullptr)) {}
  LuaFunction& operator=(LuaFunction&& other) = default;
  
  ReturnType operator()(Args... args) const {
    luaHelper::push(L, luaFunction);
    (luaHelper::push(L, std::forward<Args>(args)), ...);
    
    lua_call(L, sizeof...(Args), (std::is_same_v<ReturnType, void> ? 0 : 1));
    if constexpr (std::is_same_v<ReturnType, void>) {
      return;
    } else {
      auto&& ret = luaHelper::pull<ReturnType>(L, -1);
      lua_pop(L, 1);
      return std::forward<decltype(ret)>(ret);
    }
  }

  ~LuaFunction() {
    if (luaFunction) {
      free(L, luaFunction);
    }
  }
};

template <class R, class... A>
struct LuaTraits<LuaFunction<R(A...)>> {
  static inline bool check(lua_State* L, int index) {
    return lua_isfunction(L, index);
  }

  static inline LuaFunction<R(A...)> pull(lua_State* L, int index) {
    return LuaFunction<R(A...)>{L, store(L, StackIndex{index})};
  }
};

}

