#include "../lib/luaHelper.hpp"
#include "../lib/luaHelper/LuaClass.hpp"
#include "pallet/functional.hpp"
#include <type_traits>
#include <new>

template <class T>
struct Traits {};

template <>
struct Traits<int> {
  static void doo(int) {}
};

template <class T>
struct Tra : public Traits<T> {};

template <class T>
concept Con = requires (T v) {
  Tra<T>::doo(v);
};


int main() {
  if constexpr (Con<double>) {
    return 0;
  } else {
    return 1;
  }
}
