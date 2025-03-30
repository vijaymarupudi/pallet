#include <cstdio>
#include "pallet/error.hpp"
#include <memory>

struct X {
  std::unique_ptr<int> v;
  X(std::unique_ptr<int>&& v) : v(std::move(v)) {};
};

int main()

{
  auto x = std::make_unique<int>(2);
  auto v = pallet::Result<X>(std::in_place, std::move(x));
  return 0;
}
