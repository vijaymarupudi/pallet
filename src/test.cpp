#include <cstdio>
#include <type_traits>
#include <utility>
#include <memory>
#include "pallet/functional.hpp"

int main()
{
  int x = 10;
  pallet::Callable<void, int> callable ([&](int){
    printf("here! %d\n", x);
  });

  pallet::Callable<void, int> other ([&](int){
    printf("here 1! %d\n", x);
  });
  
  other = std::move(callable);
  other(3);
}
    
