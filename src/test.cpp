#include <cstdio>
#include "pallet/Event.hpp"


int main()

{
  pallet::Event<void, int> event;

  for (int i = 0; i < 20; i++) {
    event.addEventListener([](int v, void* ud) {
      printf("HERE! %d %ld\n", v, reinterpret_cast<intptr_t>(ud));
      (void)ud;
    }, reinterpret_cast<void*>((intptr_t)i));
  }

  event.emit(3);
  // auto vec = pallet::containers::FlexibleVector<int, 4>{};
  // vec.emplace_back(2);
  // return vec[0];
}
