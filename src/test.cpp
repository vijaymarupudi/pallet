// #include <print>
#include "../lib/luaHelper.hpp"
#include "pallet/functional.hpp"

#include <type_traits>

void other(lua_State* L) {
  pallet::luaHelper::push(L, [&](lua_State*, int, int) {
    return 0;
  });
}
int main() {
 
}
