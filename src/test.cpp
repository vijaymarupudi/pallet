#include <cstdio>
#include "pallet/functional.hpp"
#include <type_traits>

int main()

{
  // auto callable = pallet::Callable<void, int>([] (int x) {
  //   printf("%d\n", x);
  // });

  // callable(3);

  int x = 3;
  auto test = [&](int y) {
    printf("%d, %d\n", y, x);
  };

  static_assert(std::is_trivially_destructible_v<decltype(test)>);
  static_assert(std::is_trivially_move_constructible_v<decltype(test)>);

  auto c = pallet::Callable<void, int>(std::move(test));

  c(4);
  

}
    
