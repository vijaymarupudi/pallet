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

namespace detail {

template <class... ArgumentType>
struct IncludesContextHelper {
  static constexpr bool value = false;
};

template <concepts::ContextRetrievable ContextType, class... ArgumentType>
struct IncludesContextHelper<ContextType, ArgumentType...> {
  static constexpr bool value = true;
};

}

template <class... ArgumentType>
concept IncludesContext = detail::IncludesContextHelper<ArgumentType...>::value;

}


template <class T>
struct RemoveContextType;

template <class ReturnType, concepts::ContextRetrievable ContextType, class... ArgumentType>
struct RemoveContextType<ReturnType(ContextType, ArgumentType...)> {
  using type = ReturnType(ArgumentType...);
};

template <class ReturnType, class... ArgumentType>
struct RemoveContextType<ReturnType(ArgumentType...)> {
  using type = ReturnType(ArgumentType...);
};

template <class T>
struct GetContextType;

template <class ReturnType, concepts::ContextRetrievable ContextType, class... ArgumentType>
struct GetContextType<ReturnType(ContextType, ArgumentType...)> {
  using type = ContextType;
};

template <class ReturnType, class... ArgumentType>
struct GetContextType<ReturnType(ArgumentType...)> {
  using type = void;
};

template <class T>
using RemoveContextTypeT = RemoveContextType<T>::type;

template <class T>
using GetContextTypeT = GetContextType<T>::type;

template <concepts::ContextRetrievable T>
decltype(auto) retrieveContext(lua_State* L) {
  return LuaRetrieveContext<T>::retrieve(L);
}

template <class... ArgumentType>
requires (!concepts::IncludesContext<ArgumentType...>)
decltype(auto) invokeWithContextArgsFromLua(lua_State* L, auto&& function) {
  return std::apply([&](auto&&... args) {
    return std::forward<decltype(function)>(function)(std::forward<ArgumentType>(args)...);
  }, checkedPullMultiple<ArgumentType...>(L));
}

template <concepts::ContextRetrievable ContextType, class... ArgumentType>
decltype(auto) invokeWithContextArgsFromLua(lua_State* L, auto&& function) {
  return std::apply([&](auto&&... args) {
    return std::forward<decltype(function)>(function)(retrieveContext<ContextType>(L), std::forward<ArgumentType>(args)...);
  }, checkedPullMultiple<ArgumentType...>(L));
}

// template <class... ArgumentType>
// requires (!concepts::IncludesContext<ArgumentType...>)
// decltype(auto) invokeWithContext(lua_State* L, auto&& function, ArgumentType&&... args) {
//   return std::forward<decltype(function)>(function)(std::forward<ArgumentType>(args)...);
// }

// template <concepts::ContextRetrievable ContextType>
// decltype(auto) invokeWithContext(lua_State* L, auto&& function, ArgumentType&&... args) {
//   return std::forward<decltype(function)>(function)(retrieveContext<ContextType>(L), std::forward<ArgumentType>(args)...);
// }


template <class T>
struct LuaClosureTraits {
  // not incomplete so that the concept check works with public inheritance
};



// Stateless default constructible [contextual] callable (non-capturing lambda)

template <class ReturnType,
          class CallableType,
          class... ArgumentType>

requires concepts::Stateless<CallableType> &&
  std::is_default_constructible_v<CallableType> &&
  concepts::HasCallOperator<CallableType> &&
  // Check that non-context arguments are pullable
  (([]<class R, class... A>(std::type_identity<R(A...)>) {
      return (concepts::Pullable<A> && ...);
    })(std::type_identity<RemoveContextTypeT<ReturnType(ArgumentType...)>>{}))
  
struct LuaClosureTraits<ReturnType(CallableType::*)(ArgumentType...)> {
  // static constexpr bool valid = true;
  static constexpr bool usesContext = concepts::IncludesContext<ArgumentType...>;
  using abstractFunctionType = ReturnType(ArgumentType...);
  using contextType = GetContextTypeT<abstractFunctionType>;
  using contextlessAbstractFunctionType = RemoveContextTypeT<abstractFunctionType>;
  using returnType = ReturnType;

  static inline lua_CFunction toPushable(auto&&) {

    auto luaCFunc = static_cast<lua_CFunction>([](lua_State* L) -> int {
      if constexpr (std::same_as<ReturnType, void>) {
        invokeWithContextArgsFromLua<ArgumentType...>(L, CallableType{});
        return 0;
      } else {
        luaHelper::push(L, invokeWithContextArgsFromLua<ArgumentType...>(L, CallableType{}));
        return 1;
      }
    });
    return luaCFunc;
  }
};


// Redirect const operator() to the non-const version
template <class ReturnType, class CallableType, class... ArgumentType>
struct LuaClosureTraits<ReturnType(CallableType::*)(ArgumentType...) const> : public LuaClosureTraits<ReturnType(CallableType::*)(ArgumentType...)> {};


// Callable (operator()) support
template <concepts::HasCallOperator T>
struct LuaClosureTraits<T> : public LuaClosureTraits<decltype(&T::operator())> {};

// Function pointer support
template <class ReturnType, class... ArgumentType, ReturnType(*funcPtr)(ArgumentType...)>
struct LuaClosureTraits<constant_wrapper<funcPtr>> : public LuaClosureTraits<decltype([](ArgumentType... args) {
  return funcPtr(std::forward<ArgumentType>(args)...);
 })> {};


namespace concepts {

// template <class T>
// concept PushableClosure = LuaClosureTraits<T>::valid;// requires(T v) {
//   LuaClosureTraits<T>::toPushable(v);
// };

template <class T>
concept PushableClosure = requires(T v) {
   LuaClosureTraits<T>::toPushable(v);
};


}

auto toPushable(auto&& input) {
  return LuaClosureTraits<std::decay_t<decltype(input)>>::toPushable(std::forward<decltype(input)>(input));
}

template <class T>
requires concepts::PushableClosure<std::decay_t<T>>
struct LuaTraits<T> {
  template <class CallableType>
  static inline void push(lua_State* L, CallableType&& val) {
    luaHelper::push(L, LuaClosureTraits<T>::toPushable(std::forward<CallableType>(val)));
  }
};

static inline decltype(auto) callWithContextIfNeeded(lua_State* L, auto&& functionType, auto&&... args) {
  using Traits = LuaClosureTraits<std::decay_t<decltype(functionType)>>;
  if constexpr (Traits::usesContext) {
    return std::forward<decltype(functionType)>(functionType)
      (retrieveContext<typename Traits::contextType>(L), std::forward<decltype(args)>(args)...);
  } else {
    return std::forward<decltype(functionType)>(functionType)(std::forward<decltype(args)>(args)...);
  }
}

template <class FunctionType, class TransformationType>
requires (concepts::Stateless<FunctionType> &&
          concepts::Stateless<TransformationType>)
static inline constexpr decltype(auto) luaClosureChained(FunctionType&&, TransformationType&&) {

  using Traits = LuaClosureTraits<std::decay_t<FunctionType>>;
        
  auto action = ([&]<class R, class... A>(std::type_identity<R(A...)>) {
      
    
      return [](lua_State* L, A... args) {
        if constexpr (std::same_as<R, void>) {
          callWithContextIfNeeded(L, FunctionType{}, std::forward<A>(args)...);
          return TransformationType{}(L);
        } else {
          return TransformationType{}(L, (callWithContextIfNeeded(L, FunctionType{},
                                                                  std::forward<A>(args)...)));
        }
      };
    });

  return action(std::type_identity<typename Traits::contextlessAbstractFunctionType>{});
}


}

