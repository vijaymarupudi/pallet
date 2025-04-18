#pragma once

#include <tuple>
#include <type_traits>
#include <concepts>


#include "lua.hpp"
#include "pallet/functional.hpp"
#include "concepts.hpp"

namespace pallet::luaHelper {


template <class ValueType>
struct LuaTraits;

namespace concepts {
template <class T>
concept Pushable = requires (lua_State* L, T value) {
  LuaTraits<std::remove_cvref_t<T>>::push(L, value);
};

template <class T>
concept Checkable = requires (lua_State* L, int index) {
  { LuaTraits<std::remove_cvref_t<T>>::check(L, index) } -> std::same_as<bool>;
};


template <class T>
concept Pullable = !std::is_rvalue_reference_v<T> && requires (lua_State* L, int index) {
  { LuaTraits<T>::pull(L, index) } -> std::same_as<T>;
};

}

static inline void push(lua_State* L, concepts::Pushable auto&& value) {
  LuaTraits<std::remove_cvref_t<decltype(value)>>::push(L, std::forward<decltype(value)>(value));
}

template <concepts::Pullable T>
static inline decltype(auto) pull(lua_State* L, int index) {
  if constexpr (std::is_rvalue_reference_v<T>) {
    // rvalue reference, invalid
    static_assert(false, "Cannot pull rvalue reference");
  } else {
    return LuaTraits<T>::pull(L, index);
  }
}

template <concepts::Checkable T>
static inline bool isType(lua_State* L, int index) {
  return LuaTraits<std::remove_cvref_t<T>>::check(L, index);
}

template <class K, class V>
static inline void rawSetTable(lua_State* L, int tableIndex, K&& key, V&& value) {
  push(L, std::forward<K>(key));
  push(L, std::forward<V>(value));
  lua_rawset(L, tableIndex);
}


template <class T>
decltype(auto) checkedPull(lua_State* L, int index) {
  if (isType<T>(L, index)) [[likely]] {
    return pull<T>(L, index);
  } else {
    luaL_argerror(L, index, "wrong argument");
    std::unreachable();
  }
}

template <class... Types>
std::tuple<Types...>
checkedPullMultiple(lua_State* L, int baseIndex = 1) {
  return ([&]<int... indexes>(std::integer_sequence<int, indexes...>) {
      return std::make_tuple(checkedPull<Types>(L, baseIndex + indexes)...);
    })(std::make_integer_sequence<int, sizeof...(Types)>{});
}

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
    return lua_type(L, index) == LUA_TSTRING;
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
    return lua_type(L, index) == LUA_TNUMBER && !lua_isinteger(L, index);
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


template <>
struct LuaTraits<const char*> {
  static inline void push(lua_State* L, const char* str) {
    lua_pushstring(L, str);
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
concept LuaContextConvertable = requires (lua_State* L) {
  { LuaRetrieveContext<T>::retrieve(L) } -> std::same_as<T>;
};


using namespace pallet::luaHelper;

template <class T>
struct CppFunctionToLuaCFunction;

template <class R, LuaContextConvertable T, class... A>
struct CppFunctionToLuaCFunction<R(T, A...)>  {
  template <class FunctionType>
  static inline constexpr lua_CFunction convert(FunctionType&& function)
    requires (concepts::Stateless<std::remove_reference_t<FunctionType>>)
  {

    // Don't need to use it, just need its type. It has no size
    (void)function;
  
    lua_CFunction func = +[](lua_State* L) -> int {

      auto l = [&](auto&&... args) {
        // This is fine, it is stateless
        auto&& context = LuaRetrieveContext<T>::retrieve(L);
        auto lambdaPtr = reinterpret_cast<std::remove_reference_t<FunctionType>*>((void*)0);
        return lambdaPtr->operator()(std::forward<decltype(context)>(context), std::forward<decltype(args)>(args)...);
      };

      auto&& argsTuple = checkedPullMultiple<A...>(L, 1);
      if constexpr (std::is_same_v<R, void>) {
        std::apply(l, std::move(argsTuple));
        return 0;
      } else {
        luaHelper::push(L, std::apply(l, std::move(argsTuple)));
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
    requires (concepts::Stateless<std::remove_reference_t<FunctionType>>)
  {
    // Don't need to use it, just need its type. It has no size
    (void)function;
  
    lua_CFunction func = +[](lua_State* L) -> int {

      auto l = [&](auto&&... args) {
        // This is fine, it is stateless
        auto lambdaPtr = reinterpret_cast<std::remove_reference_t<FunctionType>*>((void*)0);
        return lambdaPtr->operator()(std::forward<decltype(args)>(args)...);
      };

      auto&& argsTuple = checkedPullMultiple<A...>(L, 1);
      if constexpr (std::is_same_v<R, void>) {
        std::apply(l, std::move(argsTuple));
        return 0;
      } else {
        luaHelper::push(L, std::apply(l, std::move(argsTuple)));
        return 1;
      }
    
    };
  
    return func;
  }
};

template <class T>
concept ConvertableFunction = concepts::HasCallOperator<std::remove_reference_t<T>> &&
                              concepts::Stateless<std::remove_reference_t<T>>;

template <class T>
lua_CFunction cppFunctionToLuaCFunction(T&& function)
  requires(ConvertableFunction<T>) {
  using StructType = CppFunctionToLuaCFunction<pallet::CallableFunctionType<std::remove_reference_t<T>>>;
  return StructType::convert(std::forward<T>(function));
}
  
}

template <class T>
requires (detail::ConvertableFunction<T>)
static inline lua_CFunction toLuaCFunction(T&& function) {
  return detail::cppFunctionToLuaCFunction(std::forward<decltype(function)>(function));
}

template <class T>
requires (requires (T value) { toLuaCFunction(value); })
struct LuaTraits<T> {
  template <class Arg>
  static inline void push(lua_State* L, Arg&& val) {
    lua_pushcfunction(L, detail::cppFunctionToLuaCFunction(std::forward<Arg>(val)));
  }
};

}
