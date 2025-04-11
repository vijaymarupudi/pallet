#include <cstdio>
#include "pallet/functional.hpp"

int main()
{


  int x = 100;
  
  pallet::Callable<void(int)> callable ([&](int){
    printf("here! %d\n", x);
  });

  pallet::Callable<void(int)> other ([&](int){
    printf("here 1! %d\n", x);
  });

  other = std::move(callable);
  other(3);

  pallet::Callable item {[&](int z) {
    printf("there %d\n", z);
    return 0;
  }};

  item(10);


  pallet::Callable<void(int)> twonother (nullptr);

  if (twonother) {
    twonother(3);
  }
  
}
