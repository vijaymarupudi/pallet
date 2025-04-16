#pragma once

#include <tuple>
#include <concepts>
#include "lua.hpp"
#include "pallet/functional.hpp"

namespace pallet::luaHelper {
template <class ValueType>
struct LuaTraits;

static inline void push(lua_State* L, auto&& value);

template <class T>
static inline bool isType(lua_State* L, int index);

template <class T>
static inline decltype(auto) pull(lua_State* L, int index);

template <class... Types>
std::tuple<Types...>
checkedPullMultiple(lua_State* L, int baseIndex = 1);

template <>
struct LuaTraits<bool> {
  static inline bool check(lua_State* L, int index) {
    return lua_isboolean(L, index);
  }

  static inline void push(lua_State* L, bool val) {
    lua_pushboolean(L, val);
  }

  static inline bool pull(lua_State* L, int index) {
    return lua_toboolean(L, index);
  }
};

template <class T>
requires (std::integral<T> || std::is_enum_v<T>)
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index) {
    return lua_isinteger(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushinteger(L, static_cast<lua_Integer>(val));
  }

  static inline T pull(lua_State* L, int index) {
    auto ret = lua_tointeger(L, index);
    return static_cast<T>(ret);
  }
};

template <class T>
requires (std::is_convertible_v<T, std::string_view>)
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index) {
    return lua_isstring(L, index);
  }

  static inline void push(lua_State* L, const std::string_view& str) {
    lua_pushlstring(L, str.data(), str.size());
  }

  static inline std::string_view pull(lua_State* L, int index) {
    size_t len = 0;
    auto str = lua_tolstring(L, index, &len);
    return std::string_view(str, len);
  }
};


template <std::floating_point T>
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index) {
    return lua_isnumber(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushnumber(L, static_cast<lua_Number>(val));
  }

  static inline T pull(lua_State* L, int index) {
    auto ret = lua_tonumber(L, index);
    return static_cast<T>(ret);
  }
};

template <>
struct LuaTraits<lua_CFunction> {
private:
  using T = lua_CFunction;
  public:
  static inline bool check(lua_State* L, int index) {
    return lua_iscfunction(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushcfunction(L, val);
  }
};

template <std::convertible_to<lua_CFunction> T>
struct LuaTraits<T> {
private:
  // using T = lua_CFunction;
  public:
  static inline bool check(lua_State* L, int index) {
    return lua_iscfunction(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushcfunction(L, val);
  }
};

template <class T>
requires std::is_pointer_v<T>
struct LuaTraits<T> {
  static inline bool check(lua_State* L, int index) {
    return lua_islightuserdata(L, index);
  }

  static inline void push(lua_State* L, T val) {
    lua_pushlightuserdata(L, val);
  }

  static inline T pull(lua_State* L, int index) {
    return static_cast<T>(lua_touserdata(L, index));
  }
};


template <class T>
struct LuaRetrieveContext;

template <>
struct LuaRetrieveContext<lua_State*> {
  static inline lua_State* retrieve(lua_State* L) {
    return L;
  }
};


namespace detail {

template <class T>
struct StatelessIntLikeHelper : T {
  int val;
};


template <class T>
concept HasCallOperator = requires {
  &T::operator();
};

template <class T>
concept Stateless = sizeof(StatelessIntLikeHelper<T>) == sizeof(int);

template <class T>
concept LuaContextConvertable = requires (lua_State* L) {
  { LuaRetrieveContext<T>::retrieve(L) } -> std::same_as<T>;
};


using namespace pallet::luaHelper;

template <class T>
struct CppFunctionToLuaCFunction;

template <class R, class T, class... A>
requires LuaContextConvertable<T>
struct CppFunctionToLuaCFunction<R(T, A...)>  {
  template <class FunctionType>
  static inline constexpr lua_CFunction convert(FunctionType&& function)
    requires (Stateless<std::remove_reference_t<FunctionType>>)
  {

    // Don't need to use it, just need its type. It has no size
    (void)function;
  
    lua_CFunction func = +[](lua_State* L) -> int {

      
      auto l = [&](A... args) {
        // This is fine, it is stateless
        auto&& context = LuaRetrieveContext<T>::retrieve(L);
        auto lambdaPtr = reinterpret_cast<std::remove_reference_t<FunctionType>*>((void*)0);
        return lambdaPtr->operator()(std::forward<decltype(context)>(context), std::move(args)...);
      };

      auto&& argsTuple = checkedPullMultiple<A...>(L);
      if constexpr (std::is_same_v<R, void>) {
        std::apply(l, std::move(argsTuple));
        return 0;
      } else {
        push(L, std::apply(l, std::move(argsTuple)));
        return 1;
      }
    
    };
  
    return func;
  }
};


// Version without context
template <class R, class... A>
struct CppFunctionToLuaCFunction<R(A...)>  {
  template <class FunctionType>
  static inline constexpr lua_CFunction convert(FunctionType&& function)
    requires (Stateless<std::remove_reference_t<FunctionType>>)
  {
    // Don't need to use it, just need its type. It has no size
    (void)function;
  
    lua_CFunction func = +[](lua_State* L) -> int {

      auto l = [&](A... args) {
        // This is fine, it is stateless
        auto lambdaPtr = reinterpret_cast<std::remove_reference_t<FunctionType>*>((void*)0);
        return lambdaPtr->operator()(std::move(args)...);
      };

      auto&& argsTuple = checkedPullMultiple<A...>(L);
      if constexpr (std::is_same_v<R, void>) {
        std::apply(l, std::move(argsTuple));
        return 0;
      } else {
        push(L, std::apply(l, std::move(argsTuple)));
        return 1;
      }
    
    };
  
    return func;
  }
};


template <class T>
lua_CFunction cppFunctionToLuaCFunction(T&& function)
  requires(HasCallOperator<std::remove_reference_t<T>> &&
           Stateless<std::remove_reference_t<T>>) {
  using StructType = CppFunctionToLuaCFunction<pallet::CallableFunctionType<std::remove_reference_t<T>>>;
  return StructType::convert(std::forward<T>(function));
}
  
}

template <class T>
requires (requires (T value) {
  detail::cppFunctionToLuaCFunction(value);
})
struct LuaTraits<T> {
  template <class Arg>
  static inline void push(lua_State* L, Arg&& val) {
    lua_pushcfunction(L, detail::cppFunctionToLuaCFunction(std::forward<Arg>(val)));
  }
};



}
