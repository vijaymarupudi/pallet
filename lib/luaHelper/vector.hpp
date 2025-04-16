#include "../luaHelper.hpp"
#include <vector>


namespace pallet::luaHelper {

template <class Type>
struct LuaTraits<std::vector<Type>> {
  static inline bool check(lua_State* L, int index) {
    return lua_istable(L, index);
  }

  static inline void push(lua_State* L, const std::vector<Type>& val) {
    lua_createtable(L, val.size(), 0);
    auto tableIndex = lua_gettop(L);
    size_t i = 0;
    for (auto&& item : val) {
      push(L, std::forward<decltype(item)>(item));
      lua_rawseti(L, tableIndex, i);
      i++;
    }
  }

  static inline std::vector<Type> pull(lua_State* L, int tableIndex) {
    auto len = lua_rawlen(L, tableIndex);
    std::vector<Type> item;
    item.reserve(len);
    for (size_t i = 0; i < len; i++) {
      lua_rawgeti(L, tableIndex, i);
      item.push_back(luaHelper::pull<Type>(L, -1));
      lua_pop(L, 1);
    }
    return item;
  }

};


}
