#pragma once
#include "pallet/containerUtils.hpp"
#include "pallet/IdTable.hpp"

namespace pallet {
template<class T>
class LocalPoolAllocator {
public:

  template <class V>
  using Space = pallet::containers::Space<V>;
  
  template <class U>
  using Container =
    std::conditional_t<pallet::constants::isEmbeddedDevice,
                       containers::StaticVector<U, 256>,
                       std::deque<U>>;

  Container<Space<T>> storage;
  Container<Space<T>*> freeList;

  template <class... Args>
  T* emplace(Args&&... args) {
    if (freeList.size() != 0) {
      Space<T>* space = freeList.back();
      space->construct(std::forward<Args>(args)...);
      return space->ptr();
    }
    
    auto& space = this->storage.emplace_back();
    space.construct(std::forward<Args>(args)...);
    return space.ptr();
  }

  void free(void* ptr) {
    auto space = reinterpret_cast<Space<T>*>(ptr);
    space->destroy();
    this->freeList.push_back(space);
  }

  ~LocalPoolAllocator() {
    std::sort(freeList.begin(), freeList.end(), [](const Space<T>* p1, const Space<T>* p2) {
      return ((uintptr_t)p1) > ((uintptr_t)p2);
    });

    for (size_t i = 0; i < this->storage.size(); i++) {
      if (freeList.size() != 0 && freeList.back() == &this->storage[i]) {
        freeList.pop_back();
      } else {
        this->storage[i].destroy();
      }
    }
  }
};
}
