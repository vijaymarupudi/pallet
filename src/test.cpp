#include <cstdio>
#include "pallet/Either.hpp"

struct Other {
  int x;
  int y;
  int z;
};

int main()
{
  pallet::Either<int, Other> item (3, 2, 3);

  item.visit(pallet::overloads {[&](int) {
    printf("Integer!\n");
  }, [&](Other) {
    printf("Other\n");
  }});
}
