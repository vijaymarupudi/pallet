#pragma once

#include "LuaTraits.hpp"
#include "type_traits.hpp"
#include "concepts.hpp"

namespace pallet::luaHelper {

template <class T, class Tag = void>
struct LuaRetrieveContext;

template <>
struct LuaRetrieveContext<lua_State*> {
  static inline lua_State* retrieve(lua_State* L) {
    return L;
  }
};

namespace concepts {
template <class T>
concept ContextRetrievable = requires (lua_State* L) {
  { LuaRetrieveContext<T>::retrieve(L) } -> std::same_as<T>;
};
}

template <concepts::ContextRetrievable T>
decltype(auto) retrieveContext(lua_State* L) {
  return LuaRetrieveContext<T>::retrieve(L);
}


namespace detail {
template <class... A>
struct IncludesContextHelper {
  static constexpr bool value = false;
};

template <concepts::ContextRetrievable C, class... A>
struct IncludesContextHelper<C, A...> {
  static constexpr bool value = true;
};

}

template <class... A>
concept IncludesContext = detail::IncludesContextHelper<A...>::value;


template <class... A>
requires (!IncludesContext<A...>)
decltype(auto) invokeWithContext(lua_State* L, auto&& function) {
  return std::apply([&](auto&&... args) {
    return std::forward<decltype(function)>(function)(std::forward<A>(args)...);
  }, checkedPullMultiple<A...>(L));
}

template <concepts::ContextRetrievable C, class... A>
// requires (IncludesContext<C,A...>)
decltype(auto) invokeWithContext(lua_State* L, auto&& function) {
  return std::apply([&](auto&&... args) {
    return std::forward<decltype(function)>(function)(retrieveContext<C>(L), std::forward<A>(args)...);
  }, checkedPullMultiple<A...>(L));
}


template <class T>
struct RemoveContextType;

template <class R, concepts::ContextRetrievable C, class... A>
struct RemoveContextType<R(C, A...)> {
  using type = R(A...);
};

template <class R, class... A>
struct RemoveContextType<R(A...)> {
  using type = R(A...);
};

template <class T>
using RemoveContextTypeT = RemoveContextType<T>::type;

template <class T>
struct LuaClosureTraits;

// Stateless default constructible [contextual] callable (non-capturing lambda)
template <concepts::Pushable R,
          class CallableType,
          class... A>
requires
  ((concepts::Pullable<A> && ...) && // arguments should be pullable
   (concepts::Stateless<CallableType> && // requires for easy stateless calling
    concepts::HasCallOperator<CallableType> &&
    std::is_trivially_default_constructible_v<CallableType>))
struct LuaClosureTraits<R(CallableType::*)(A...)> {
  
  static constexpr bool usesContext = IncludesContext<A...>;
  using contextlessAbstractFunctionType = RemoveContextTypeT<R(A...)>;
  using returnType = R;

  static inline void push(lua_State* L, auto&&) {
    auto luaCFunc = static_cast<lua_CFunction>([](lua_State* L) -> int {
      CallableType callable;
      auto&& result = invokeWithContext<A...>(L, std::move(callable));
      if constexpr (std::same_as<R, void>) {
        return 0;
      } else {
        luaHelper::push(L, std::forward<decltype(result)>(result));
        return 1;
      }
      
      
    });
    luaHelper::push(L, luaCFunc);
  }
};

template <class R,
          class CallableType,
          class... A>
struct LuaClosureTraits<R(CallableType::*)(A...) const> : public LuaClosureTraits<R(CallableType::*)(A...)> {};


template <concepts::HasCallOperator T>
struct LuaClosureTraits<T> : public LuaClosureTraits<decltype(&T::operator())> {};

template <class R, class... A, R(*funcPtr)(A...)>
struct LuaClosureTraits<constant_wrapper<funcPtr>> : public LuaClosureTraits<decltype([](A... args) {
  return funcPtr(std::forward<A>(args)...);
 })> {};





// // Contextual function pointer
// template <concepts::Pushable R, concepts::ContextRetrievable C, class... A>
// requires (concepts::Pullable<A> && ...)
// struct LuaClosureTraits<R(*)(C, A...)> {
  
//   static constexpr bool usesContext = true;
//   using contextlessAbstractFunctionType = R(A...);
  
//   template <R(*userFunc)(C, A...)>
//   static inline void push(lua_State* L, constant_wrapper<userFunc>) {
//     auto luaCFunc = static_cast<lua_CFunction>([](lua_State* L) -> int {
//       std::apply([&](A... args) {
//         auto&& context = retrieveContext(L);
//         auto&& result = userFunc(std::forward<decltype(context)>(context), std::forward<A>(args)...);
//         luaHelper::push(L, std::forward<decltype(result)>(result));
//       }, checkedPullMultiple<A...>(L));
//       return 1;
//     });
//     luaHelper::push(L, luaCFunc);
//   }
// };


// // Non-contextual function pointer
// template <concepts::Pushable R, class... A>
// requires (concepts::Pullable<A> && ...)
// struct LuaClosureTraits<R(*)(A...)> {
  
//   static constexpr bool usesContext = false;
//   using contextlessAbstractFunctionType = R(A...);

//   template <R(*userFunc)(A...)>
//   static inline void push(lua_State* L, constant_wrapper<userFunc>) {
    
//     auto luaCFunc = static_cast<lua_CFunction>([](lua_State* L) -> int {
//       std::apply([&](A... args) {
//         auto&& result = userFunc(std::forward<A>(args)...);
//         luaHelper::push(L, std::forward<decltype(result)>(result));
//       }, checkedPullMultiple<A...>(L));
//       return 1;
//     });
    
//     luaHelper::push(L, luaCFunc);
//   }
// };

namespace detail {

using namespace pallet::luaHelper;

template <class T>
struct CppFunctionToLuaCFunction;

template <class R, concepts::ContextRetrievable T, class... A>
struct CppFunctionToLuaCFunction<R(T, A...)>  {

  static constexpr bool usesContext = true;
  using ContextType = T;
  
  template <class FunctionType>
  static inline constexpr lua_CFunction convert(FunctionType&&)
    requires (concepts::Stateless<std::remove_reference_t<FunctionType>>)
  {
    
    lua_CFunction func = +[](lua_State* L) -> int {

      auto l = [&](auto&&... args) {
        // This is fine, it is stateless
        auto&& context = LuaRetrieveContext<T>::retrieve(L);
        auto lambda = std::decay_t<FunctionType>{};
        return std::move(lambda)(std::forward<decltype(context)>(context), std::forward<decltype(args)>(args)...);
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

  static constexpr bool usesContext = false;
  
  template <class FunctionType>
  static inline constexpr lua_CFunction convert(FunctionType&& function)
    requires (concepts::Stateless<std::remove_reference_t<FunctionType>>)
  {
    // Don't need to use it, just need its type. It has no size
    (void)function;
  
    lua_CFunction func = +[](lua_State* L) -> int {

      auto l = [&](auto&&... args) {
        // This is fine, it is stateless
        auto lambda = std::decay_t<FunctionType>{};
        return std::move(lambda)(std::forward<decltype(args)>(args)...);
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

} // namespace detail

template <class T>
requires (detail::ConvertableFunction<T>)
static inline lua_CFunction toLuaCFunction(T&& function) {
  return detail::cppFunctionToLuaCFunction(std::forward<decltype(function)>(function));
}

// template <class FunctionType, class... Args>
// decltype(auto) invokeWithContext(lua_State* L, FunctionType&& function, Args&&... args) {

//   using InfoType = detail::CppFunctionToLuaCFunction<
//                                  pallet::CallableFunctionType<
//                                    std::remove_reference_t<FunctionType>>>;

//   constexpr bool usesContext = InfoType::usesContext;

//   if constexpr (usesContext) {
//     auto&& context = LuaRetrieveContext<typename InfoType::ContextType>::retrieve(L);
//     return std::forward<decltype(function)>(function)(std::forward<decltype(context)>(context), std::forward<Args>(args)...);
//   } else {
//     // return [&](auto&& context) -> decltype(auto) {
      
//     // }
//     return std::forward<decltype(function)>(function)(std::forward<Args>(args)...);
//   }
// }

template <class T>
requires (requires (T value) { toLuaCFunction(value); })
struct LuaTraits<T> {
  template <class Arg>
  static inline void push(lua_State* L, Arg&& val) {
    lua_pushcfunction(L, detail::cppFunctionToLuaCFunction(std::forward<Arg>(val)));
  }
};

}
