#pragma once

namespace pallet {
auto min(auto& a, auto& b) {
  if (a < b) { return a; }
  return b;
}
}
