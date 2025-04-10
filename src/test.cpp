#include <cstdio>
#include <type_traits>
#include <utility>
#include "pallet/io.hpp"

int main()
{
  using namespace pallet;
  std::string out = pallet::unwrap(readFile("src/test.cpp"));
  printf("%s\n", out.c_str());
}
    
