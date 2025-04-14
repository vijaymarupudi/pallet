#include <cstdio>
#include "pallet/LightVariant.hpp"

int main()
{
  using namespace pallet;
  using Var = LightVariant<int, double, std::string>;
  Var v = 3.0;
  Var o = v;
  (void)o;
}
