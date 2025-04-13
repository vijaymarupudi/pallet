#include <cstdio>
#include "pallet/LightVariant.hpp"
#include <string>

struct T1 {};
struct T2 {};
struct T3 {};

int main()
{
  using Var = pallet::LightVariant<int, double>;
  Var v (3.0);

  v = Var(1);

  auto res = v.get_if<int>();
  printf("val: %d\n", *res);

  pallet::visit(pallet::overloads {
      [&](int&) {
        printf("int\n");
      },
        [&](double&) {
          printf("double\n");
        }// ,
        // [&](std::string&) {
        //   printf("string\n");
        // }
        }, v);
}
