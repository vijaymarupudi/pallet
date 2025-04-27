#pragma once

#include "LuaTraits.hpp"
#include "type_traits.hpp"
#include "finalizable.hpp"
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

template <class T>
concept IsStatelessFunction = concepts::Stateless<std::decay_t<T>> && std::is_default_constructible_v<std::decay_t<T>>;
}

namespace detail {

template <class T>
struct ContextTraits {};

template <class ReturnType, class... ArgumentType>
struct ContextTraits<ReturnType(ArgumentType...)> {
  static constexpr bool usesContext = false;
  using functionWithoutContextType = ReturnType(ArgumentType...);
  static constexpr bool normalized = false;
};

template <class ReturnType, concepts::ContextRetrievable ContextType, class... ArgumentType>
struct ContextTraits<ReturnType(ContextType, ArgumentType...)> {
  static constexpr bool usesContext = true;
  using contextType = ContextType;
  using functionWithoutContextType = ReturnType(ArgumentType...);
  static constexpr bool normalized = false;
};

template <class ReturnType, class... ArgumentType>
struct ContextTraits<ReturnType(lua_State*, ArgumentType...)> {
  static constexpr bool usesContext = true;
  using contextType = lua_State*;
  using functionWithoutContextType = ReturnType(ArgumentType...);
  static constexpr bool normalized = true;
};


}

template <concepts::ContextRetrievable T>
decltype(auto) retrieveContext(lua_State* L) {
  return LuaRetrieveContext<T>::retrieve(L);
}

namespace detail {

template <class Traits>
static inline decltype(auto) normalizeCallableApply(auto&& function, lua_State* L, auto&&... argument) {
  if constexpr (Traits::usesContext) {
    return std::forward<decltype(function)>(function)(retrieveContext<typename Traits::contextType>(L), std::forward<decltype(argument)>(argument)...);
  } else {
    return std::forward<decltype(function)>(function)(std::forward<decltype(argument)>(argument)...);
  }
}

}

namespace concepts {
template <class T>
concept FunctionArgumentsPullable = (([]<class R, class... A>(std::in_place_type_t<R(A...)>) {
      return (Pullable<A> && ...);
    })(std::in_place_type<T>));
}

template <class CallableType>
static inline decltype(auto) normalizeCallable(CallableType&& callable) {

  using Traits = detail::ContextTraits<FunctionType<CallableType>>;

  return ([&]<class ReturnType, class... ArgumentType>(std::in_place_type_t<ReturnType(ArgumentType...)>) {

      static_assert(concepts::FunctionArgumentsPullable<ReturnType(ArgumentType...)>);

      if constexpr (Traits::normalized) {
        return callable;
      } else if constexpr (concepts::IsStatelessFunction<CallableType>) {
        return [](lua_State* L, ArgumentType... argument) {
          return detail::normalizeCallableApply<Traits>(std::decay_t<CallableType>{}, L, std::forward<ArgumentType>(argument)...);
        };
      } else {
        return [callable=std::forward<CallableType>(callable)](lua_State* L, ArgumentType... argument) {
          return detail::normalizeCallableApply<Traits>(callable, L, std::forward<ArgumentType>(argument)...);
        };
      }

    })(std::in_place_type<typename Traits::functionWithoutContextType>);

}


template <class T>
struct LuaClosureTraits {
  // not an incomplete type so that the concept check works with public inheritance
};

namespace detail {

static inline int handleNormalizedCallableInLuaFunction(lua_State* L, auto&& callable) {
  return [&]<class R, class... A>(std::in_place_type_t<R(lua_State*, A...)>) {
    return std::apply([&](auto&&... args) {
      if constexpr (std::same_as<R, void>) {
        std::forward<decltype(callable)>(callable)(L, std::forward<decltype(args)>(args)...);
        return 0;
      } else {
        luaHelper::push(L, std::forward<decltype(callable)>(callable)(L, std::forward<decltype(args)>(args)...));
        return 1;
      }
    }, checkedPullMultiple<A...>(L));
  }(std::in_place_type<FunctionType<decltype(callable)>>);
}

}

// Stateless default constructible [contextual] callable (non-capturing lambda)
template <class ReturnType,
          class CallableType,
          class... ArgumentType>
struct LuaClosureTraits<ReturnType(CallableType::*)(ArgumentType...)> {

  static inline void push(lua_State* L, auto&& callable) {

    auto normalizedCallable = normalizeCallable(std::forward<decltype(callable)>(callable));

    using NormalizedCallableType = decltype(normalizedCallable);

    if constexpr (concepts::IsStatelessFunction<NormalizedCallableType>) {
      auto func = +[](lua_State* L) {
        return detail::handleNormalizedCallableInLuaFunction(L, NormalizedCallableType{});
      };
      luaHelper::push(L, func);
    } else {
      createFinalizableUserData<NormalizedCallableType>(L, std::move(normalizedCallable));
      auto func = +[](lua_State* L) {
        auto& normalizedCallable = *luaHelper::pull<NormalizedCallableType*>(L, lua_upvalueindex(1));
        return detail::handleNormalizedCallableInLuaFunction(L, normalizedCallable);
      };
      lua_pushcclosure(L, func, 1);
    }
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

template <class T>
concept PushableClosure = requires(lua_State* L, T val) {
  LuaClosureTraits<T>::push(L, std::move(val));
};

};

template <class T>
requires concepts::PushableClosure<std::decay_t<T>>
struct LuaTraits<T> {
  template <class CallableType>
  static inline void push(lua_State* L, CallableType&& val) {
    LuaClosureTraits<T>::push(L, std::forward<CallableType>(val));
  }
};


namespace detail {

static inline decltype(auto) luaFunctionPipe(lua_State* L, auto&& after, auto&& before, auto&&... args) {
  using ReturnValue = decltype(std::forward<decltype(before)>(before)(L, std::forward<decltype(args)>(args)...));
  if constexpr (std::same_as<ReturnValue, void>) {
    std::forward<decltype(before)>(before)(L, std::forward<decltype(args)>(args)...);
    return std::forward<decltype(after)>(after)(L);
  } else {
    return std::forward<decltype(after)>(after)(L, std::forward<decltype(before)>(before)(L, std::forward<decltype(args)>(args)...));
  }
}

}

template <class InputFunctionType, class TransformationType>
static inline constexpr auto luaClosureChain(InputFunctionType&& function, TransformationType&& transformation) {

  auto normalizedFunction = normalizeCallable(std::forward<InputFunctionType>(function));
  using NormalizedFunctionType = decltype(normalizedFunction);

  return ([&]<class R, class... A>(std::in_place_type_t<R(lua_State*, A...)>) {

      if constexpr (concepts::IsStatelessFunction<NormalizedFunctionType> && concepts::IsStatelessFunction<TransformationType>) {
        return [](lua_State* L, A... arg) -> decltype(auto) {
          return detail::luaFunctionPipe(L, std::decay_t<TransformationType>{}, std::decay_t<NormalizedFunctionType>{}, std::forward<A>(arg)...);
        };
      } else if constexpr (concepts::IsStatelessFunction<NormalizedFunctionType>) {
        return [transformation=std::forward<decltype(transformation)>(transformation)](lua_State* L, A... arg) -> decltype(auto) {
          return detail::luaFunctionPipe(L, transformation, std::decay_t<NormalizedFunctionType>{}, std::forward<A>(arg)...);
        };
      } else if constexpr (concepts::IsStatelessFunction<TransformationType>) {
        return [normalizedFunction=std::move(normalizedFunction)](lua_State* L, A... arg) -> decltype(auto) {
          return detail::luaFunctionPipe(L, std::decay_t<TransformationType>{}, normalizedFunction, std::forward<A>(arg)...);
        };
      } else {
        return [normalizedFunction=std::move(normalizedFunction),
                transformation=std::forward<decltype(transformation)>(transformation)](lua_State* L, A... arg) -> decltype(auto) {
          return detail::luaFunctionPipe(L, transformation, normalizedFunction, std::forward<A>(arg)...);
        };
      }
    })(std::in_place_type<FunctionType<std::decay_t<NormalizedFunctionType>>>);
}


}
