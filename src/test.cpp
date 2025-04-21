#include "../lib/luaHelper.hpp"
#include "../lib/luaHelper/LuaClass.hpp"
#include "pallet/functional.hpp"
#include <type_traits>
#include <new>


namespace {
using namespace pallet::luaHelper;

void print_value(auto input) {
  (pallet::overloaded {
    [&]<class R, class L, class A>(R(L::*)(A) const) {
      A v = 3;
      printf("%d\n", v);
    },
    [&]<class R, class L, class A>(R(L::*)(A)) {
      A v = 3;
      printf("%d\n", v);
    },
    [&]<class R, class A>(R(*)(A)) {
      A v = 3;
      printf("%d\n", v);
    }
  })(input.value);

  // ([&](int(*)(int)) {
  //   int v = 3;
  //   printf("%d\n", v);
  // })(input);

}

struct Test {
  int method(int) const {
    return 0;
  }
};

int func(int) { return 0; }

void other() {
  // static_assert(std::is_same_v<std::decay_t<decltype(func)>, int(*)(int)>);
  print_value(cw<func>);
}

}

int main() {
  other();
}
