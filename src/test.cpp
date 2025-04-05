#include <cstdio>
#include "pallet/containers/IdTable.hpp"


int main()

{
  pallet::containers::IdTable<int> table;

  table.push(1);
  auto id = table.push(2);
  table.push(3);
  auto id2 = table.push(4);
  table.push(5);
  table.free(id);
  table.free(id2);

  for (const auto& v : table) {
    printf("%d\n", v);
  }
}
