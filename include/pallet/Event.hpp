#pragma once

#include <deque>
#include <utility>
#include <cstdint>

#include "pallet/containers/IdTable.hpp"
#include "pallet/containers/FlexibleVector.hpp"
#include "pallet/functional.hpp"

namespace pallet {

template <class FunctionType, class IdType = uint32_t>
class Event;

template <class R, class... A, class IdType>
class Event<R(A...), IdType> {
public:
  using ReturnType = void;
  using Callback = pallet::Callable<ReturnType(A...)>;
  using Id = IdType;

  Id listen(Callback cb);
  void unlisten(Id id);
  template <class... Args>
  void emit(Args&&... args);

private:

  // a deque never moves existing elements when it grows, so a callback
  // can listen while emit is running the callbacks stored in it
  template <class T>
  using Storage = std::deque<T>;
  containers::IdTable<Callback, Storage, Id> items;
  // Callbacks may unlisten (even themselves) while emit is running, so
  // freeing is postponed until the outermost emit returns.
  int emitDepth = 0;
  containers::FlexibleVector<Id, 4> pendingFree;

  bool isPendingFree(Id id) {
    for (size_t i = 0; i < pendingFree.size(); i++) {
      if (pendingFree[i] == id) { return true; }
    }
    return false;
  }
};

template <class R, class... A, class IdType>
IdType Event<R(A...), IdType>::listen(Callback cb) {
  return items.emplace(std::move(cb));
}


template <class R, class... A, class IdType>
void Event<R(A...), IdType>::unlisten(Id id) {
  if (!items.contains(id) || isPendingFree(id)) { return; }
  if (emitDepth > 0) {
    pendingFree.emplace_back(id);
  } else {
    items.free(id);
  }
}

template <class R, class... A, class IdType>
template <class... Args>
void Event<R(A...), IdType>::emit(Args&&... args) {
  emitDepth++;
  // Callbacks added during this emit are not called by it, except
  // those that reuse a free slot.
  for (size_t i = 0, n = items.slots(); i < n; i++) {
    Id id = static_cast<Id>(i);
    if (items.contains(id) && !isPendingFree(id)) {
      items[id](args...);
    }
  }
  if (--emitDepth == 0) {
    for (size_t i = 0; i < pendingFree.size(); i++) {
      items.free(pendingFree[i]);
    }
    pendingFree.clear();
  }

  // if some of the arguments were moved, they will be destroyed here
}



}
