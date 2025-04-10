#include <cstdio>
#include <type_traits>
#include <utility>
#include <print>
#include "pallet/io.hpp"

int main()
{
  std::string out = pallet::unwrap(pallet::readFile("src/test.cpp"),
                                   "Cannot find file!");
  std::print("{}", out);
}
    
