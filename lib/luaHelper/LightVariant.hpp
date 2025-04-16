#pragma once

#include "../luaHelper.hpp"
#include "pallet/LightVariant.hpp"

namespace pallet::luaHelper {

  namespace detail {
    template <class T>
    struct luaPullLightVariant;

    template <class... Types>
    struct luaPullLightVariant<LightVariant<Types...>> {
      using VariantType = LightVariant<Types...>;

      template <class First, class... ArgTypes>
      static inline VariantType recursiveApplyMany(lua_State* L, int index) {
        if (isType<First>(L, index)) {
          return pull<First>(L, index);
        } else if constexpr (sizeof...(ArgTypes) == 0) {
            luaL_argerror(L, index, "wrong argument");
            std::unreachable();
        } else {
          return recursiveApplyMany<ArgTypes...>(L, index);
        }
      }
    
      static inline VariantType apply(lua_State* L, int index) {
        return recursiveApplyMany<Types...>(L, index);
      }
    };
  }

template <class... Types>
struct LuaTraits<LightVariant<Types...>> {
  static inline bool check(lua_State* L, int index) {
    return (isType<Types>(L, index) || ...);
  }

  static inline void push(lua_State* L, auto&& val) {
    pallet::visit([&](auto&& item) {
      push(L, std::forward<decltype(val)>(val));
    }, std::forward<decltype(val)>(val));
  }

  static inline LightVariant<Types...> pull(lua_State* L, int index) {
    return detail::luaPullLightVariant<LightVariant<Types...>>::apply(L, index);
  }
};

  
}
