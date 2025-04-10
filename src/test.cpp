#include <cstdio>
#include <type_traits>
#include <utility>
#include <print>
#include "pallet/io.hpp"

int main()
{
  using namespace pallet;
  std::string out = unwrap(readFile("src/test.cpp"));
  std::print("{}", out);
}
    
