#include <cstdio>
#include <type_traits>
#include <utility>
#include "pallet/functional.hpp"

int main()

{
  // record t ([](int){ return 0; });
  // int x = t;

  pallet::Callable<int, int> callable ([](int) { return 0; });

  callable(3);
  //  int t = a;
  // auto callable = pallet::Callable<void, int>([] (int x) {
  //   printf("%d\n", x);
  // });

  // callable(3);

  

}
    
