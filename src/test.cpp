#include <cstdio>
#include <string>
#include "pallet/LightVariant.hpp"
#include "../lib/luaHelper.hpp"
#include "../lib/luaHelper/variant.hpp"

auto VijayFunction(lua_State* L) {
  return pallet::luaHelper::pull<pallet::Variant<int, double>>(L, 1);
}

int main()
{
  using namespace pallet;
  using Var = LightVariant<int, double, std::string>;
  Var v = 3.0;
  Var o = v;
  o = "Hello there!\n";
  o.visit([&](auto&& s) {
    if constexpr (std::is_same_v<decltype(s)&, std::string&>) {
      printf("%s\n", s.c_str());
    }
  });
}
