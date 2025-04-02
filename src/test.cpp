#include <cstdio>
#include "pallet/error.hpp"
#include <memory>
#include "pallet/memory.hpp"


int main()

{
  pallet::UniqueResource<int, decltype([](int fd){printf("here!\n");(void)fd;})> resource {3};
  return 0;
}
