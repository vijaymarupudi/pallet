// #include <print>
#include "../lib/luaHelper.hpp"
#include "pallet/functional.hpp"

#include <type_traits>


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
concept Stateless = sizeof(detail::StatelessIntLikeHelper<T>) == sizeof(int);

template <class T>
concept LuaContextConvertable = requires (lua_State* L) {
  { LuaRetrieveContext<T>::retrieve(L) } -> std::same_as<T>;
};


using namespace pallet::luaHelper;

template <class T>
struct LambdaToLuaCFunction;

template <class R, class T, class... A>
requires LuaContextConvertable<T>
struct LambdaToLuaCFunction<R(T, A...)>  {
  template <class LambdaType>
  static inline constexpr lua_CFunction convert(LambdaType&& lambda)
    requires (Stateless<std::remove_reference_t<LambdaType>>)
  {

    // Don't need to use it, just need its type. It has no size
    (void)lambda;
  
    lua_CFunction func = +[](lua_State* L) -> int {

      auto l = [&](A... args) {
        // This is fine, it is stateless
        auto&& context = LuaRetrieveContext<T>::retrieve(L);
        auto lambdaPtr = reinterpret_cast<std::remove_reference_t<LambdaType>*>((void*)0);
        return lambdaPtr->operator()(std::forward<decltype(context)>(context), args...);
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
lua_CFunction lambdaToLuaCFunction(T&& lambda)
  requires(HasCallOperator<std::remove_reference_t<T>> &&
           Stateless<std::remove_reference_t<T>>) {
  using StructType = LambdaToLuaCFunction<pallet::CallableFunctionType<std::remove_reference_t<T>>>;
  return StructType::convert(std::forward<T>(lambda));
}

  
}

int main() {
  
  auto l = [&](lua_State*, int, double, std::string_view) {
    return 12;
  };

  auto func = detail::lambdaToLuaCFunction(l);
}
