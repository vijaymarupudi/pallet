#include <utility>
#include <cstdint>

#include "pallet/containers/IdTable.hpp"
#include "pallet/containers/FlexibleVector.hpp"
#include "pallet/functional.hpp"

namespace pallet {
template <class... A>
class Event {
public:
  using ReturnType = void;
  using Callback = pallet::Callable<ReturnType(A...)>;

  uint32_t listen(Callback cb);
  void unlisten(uint32_t id);
  template <class... Args>
  void emit(Args&&... args);

private:

  template <class T>
  using Storage = containers::FlexibleVector<T, 4>;
  containers::IdTable<Callback, Storage, uint32_t> items;
};

  template <class... A>
  uint32_t Event<A...>::listen(Callback cb) {
    return items.emplace(std::move(cb));
  }


  template <class... A>
  void Event<A...>::unlisten(uint32_t id) {
    items.free(id);
  }

  template <class... A>
  template <class... Args>
  void Event<A...>::emit(Args&&... args) {
    for (auto& callback : items) {
      callback(args...);
    }

    // if some of the arguments were moved, they will be destroyed here
  }



}
