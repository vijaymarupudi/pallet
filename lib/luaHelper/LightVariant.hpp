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
          return luaHelper::pull<First>(L, index);
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

  static inline void push(lua_State* L, const auto& variant) {
    pallet::visit([&](auto&& item) {
      luaHelper::push(L, std::forward<decltype(item)>(item));
    }, std::forward<decltype(variant)>(variant));
  }

  static inline LightVariant<Types...> pull(lua_State* L, int index) {
    return detail::luaPullLightVariant<LightVariant<Types...>>::apply(L, index);
  }
};

  
}
