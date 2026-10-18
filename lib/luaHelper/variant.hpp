#pragma once

#include "../luaHelper.hpp"
#include "pallet/variant.hpp"

namespace pallet::luaHelper {

  namespace detail {
    template <class T>
    struct luaPullVariant;

    template <class... Types>
    struct luaPullVariant<Variant<Types...>> {
      using VariantType = Variant<Types...>;

      // Construct by alternative index, so a pull() that returns a type
      // other than the alternative cannot select a different alternative
      template <size_t I, class First, class... ArgTypes>
      static inline VariantType recursiveApplyMany(lua_State* L, int index) {
        if (isType<First>(L, index)) {
          return VariantType(std::in_place_index<I>, luaHelper::pull<First>(L, index));
        } else if constexpr (sizeof...(ArgTypes) == 0) {
            luaL_argerror(L, index, "wrong argument");
            std::unreachable();
        } else {
          return recursiveApplyMany<I + 1, ArgTypes...>(L, index);
        }
      }
    
      static inline VariantType apply(lua_State* L, int index) {
        return recursiveApplyMany<0, Types...>(L, index);
      }
    };
  }

template <class... Types>
struct LuaTraits<Variant<Types...>> {
  static inline bool check(lua_State* L, int index) {
    return (isType<Types>(L, index) || ...);
  }

  static inline void push(lua_State* L, auto&& val) {
    pallet::visit([&](auto&& item) {
      luaHelper::push(L, std::forward<decltype(item)>(item));
    }, std::forward<decltype(val)>(val));
  }

  static inline Variant<Types...> pull(lua_State* L, int index) {
    return detail::luaPullVariant<Variant<Types...>>::apply(L, index);
  }
};

}
