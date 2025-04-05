#include <utility>
#include <cstdint>
#include "pallet/containers/IdTable.hpp"
#include "pallet/containers/FlexibleVector.hpp"

namespace pallet {
template <class R, class... A>
class Event {
  using cb_type = R(*)(A..., void*);
  using cb_data = std::pair<cb_type, void*>;
  template <class T>
  using storage_type = containers::FlexibleVector<T, 4>;
  containers::IdTable<cb_data, storage_type, uint32_t> items;

public:

  uint32_t addEventListener(cb_type cb, void* ud) {
    return items.emplace(cb, ud);
  }

  void removeEventListener(uint32_t id) {
    items.free(id);
  }

  template <class... Args>
  void emit(Args&&... args) {
    for (const auto& [cb, ud] : items) {
      cb(args..., ud);
    }
    // if some of the arguments were moved, they will be destroyed here
  }

};

}


