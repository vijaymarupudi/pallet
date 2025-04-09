#include <cstdio>
#include <type_traits>
#include <utility>
#include <print>
#include "pallet/io.hpp"

int main()
{
  auto res = pallet::readFile("src/test.cpp").transform([&](auto&& items) {
    std::println("{}", items);
  });

  if (!res) {
    std::println("{}", res.error().message());
  }
  
}
    
