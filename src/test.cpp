#include <cstdio>
#include "pallet/Event.hpp"


int main()

{
  auto event = pallet::Event<int>{};

  event.listen({+[](int x, void*) {
    printf("%d\n", x);
  }, nullptr});

  event.emit(3);


}
