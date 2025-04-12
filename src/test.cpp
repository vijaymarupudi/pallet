#include <cstdio>
#include "pallet/MyVariant.hpp"
#include <memory>
#include <string>

struct T1 {};
struct T2 {};
struct T3 {};

int main()
{
  using Var = pallet::MyVariant<int, double, std::string>;
  Var v (3.0);

  v = Var(1);
  v.visit(pallet::overloads {
      [&](int&) {
        printf("int\n");
      },
        [&](double&) {
          printf("double\n");
        },
        [&](std::string&) {
          printf("string\n");
        }
        });
}
