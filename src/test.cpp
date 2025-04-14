#include <cstdio>
#include "pallet/LightVariant.hpp"
#include <memory>

int main()
{
  Wrapper v (std::unique_ptr<int>(new int{3}));
}
