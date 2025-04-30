#pragma once

#include <concepts>
#include "LuaTraits.hpp"

namespace pallet::luaHelper {


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

struct LuaNilType {};

const constexpr LuaNilType LuaNil;

template<>
struct LuaTraits<LuaNilType> {
  static inline bool check(lua_State* L, int index) {
    return lua_type(L, index) == LUA_TNIL;
  }

  static inline void push(lua_State* L, auto&&) {
    lua_pushnil(L);
  }

  static inline LuaNilType pull(lua_State*, int) {
    return LuaNil;
  }
};


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

static inline void store(lua_State* L, const void* key, auto&& item) {
  luaHelper::push(L, item);
  lua_rawsetp(L, LUA_REGISTRYINDEX, key);
};

static inline void registryPush(lua_State* L, const void* key) {
  lua_rawgetp(L, LUA_REGISTRYINDEX, key);
}

template <class T>
static inline T pull(lua_State* L, const void* key) {
  registryPush(L, key);
  return luaHelper::pull<T>(L, -1);
}


static inline void free(lua_State* L, RegistryIndex index) {
  luaL_unref(L, LUA_REGISTRYINDEX, index.getIndex());
}

static inline void free(lua_State* L, const void* key) {
  luaHelper::push(L, LuaNil);
  lua_rawsetp(L, LUA_REGISTRYINDEX, key);
}

template <class T>
static inline T pull(lua_State* L, RegistryIndex index) {
  luaHelper::push(L, index);
  auto&& val = luaHelper::pull<T>(L, -1);
  lua_pop(L, 1);
  return val;
}

template <class T>
static inline T pull(lua_State* L, StackIndex index) {
  return luaHelper::pull<T>(L, index.getIndex());
}

}
