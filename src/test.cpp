#include "../lib/luaHelper.hpp"
#include "../lib/luaHelper/LuaClass.hpp"
#include "pallet/functional.hpp"
#include <type_traits>
#include <new>


namespace {

using namespace pallet::luaHelper;

// template <class R, class A, R(*value)(A)>
// void print_value(constant_wrapper<value> input) {
//   printf("%p\n", (void*)input.value);
// }

// template <class R, class L, class... A, R(L::*value)(A...)>
// void print_value(constant_wrapper<value> input) {
//   printf("%p\n", (void*)input.value);
// }

// struct Test {
//   int method(int) {
//     return 0;
//   }
// };

// int func(int) {
//   return 0;
// }


extern "C" void other(lua_State* L) {
  (void)L;

  int x = 0;
  auto func = [item = &x]() -> int {
    return *item;
  };

  static_assert(std::is_trivially_move_constructible_v<decltype(func)>);

    
  
  // static_assert(std::is_same_v<std::decay_t<decltype(func)>, int(*)(int)>);
  // print_value(cw<&Test::method>);
}

}

int main() {
  
}
