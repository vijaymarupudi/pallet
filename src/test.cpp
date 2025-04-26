#include "../lib/luaHelper.hpp"
#include "../lib/luaHelper/LuaClass.hpp"
#include "pallet/functional.hpp"
#include <type_traits>
#include <new>
#include <cstdio>

template <size_t n>
static int func(const char (&in) [n]) {
  printf("len: %lu | %s\n", n, in);
  return 0;
}

int main() {
  auto&& item = "what is the world?";
  func(item);
}
