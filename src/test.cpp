// #include "../lib/luaHelper.hpp"
// #include "../lib/luaHelper/LuaClass.hpp"
// #include "pallet/functional.hpp"
// #include <type_traits>
// #include <new>
#include <cstdio>
#include <map>
#include <expected>
#include <system_error>

template <class T>
struct Value {
  int v;
};

int main() {
  std::map<int, Value<void(std::expected<int, std::error_condition>)>> m;

  auto it = m.find(3);

  if (it == m.end()) {
    printf("Not there!\n");
  }
}
