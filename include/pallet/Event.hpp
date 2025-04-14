#include <utility>
#include <cstdint>

#include "pallet/containers/IdTable.hpp"
#include "pallet/containers/FlexibleVector.hpp"
#include "pallet/functional.hpp"

namespace pallet {

template <class FunctionType, class IdType = uint32_t, size_t staticLength = 4>
class Event;

template <class R, class... A, class IdType, size_t staticLength>
class Event<R(A...), IdType, staticLength> {
public:
  using ReturnType = void;
  using Callback = pallet::Callable<ReturnType(A...)>;
  using Id = IdType;

  Id listen(Callback cb);
  void unlisten(Id id);
  template <class... Args>
  void emit(Args&&... args);

private:

  template <class T>
  using Storage = containers::FlexibleVector<T, staticLength>;
  containers::IdTable<Callback, Storage, Id> items;
};

template <class R, class... A, class IdType, size_t staticLength>
IdType Event<R(A...), IdType, staticLength>::listen(Callback cb) {
  return items.emplace(std::move(cb));
}


template <class R, class... A, class IdType, size_t staticLength>
void Event<R(A...), IdType, staticLength>::unlisten(Id id) {
  items.free(id);
}

template <class R, class... A, class IdType, size_t staticLength>
template <class... Args>
void Event<R(A...), IdType, staticLength>::emit(Args&&... args) {
  for (auto& callback : items) {
    callback(args...);
  }

  // if some of the arguments were moved, they will be destroyed here
}



}
