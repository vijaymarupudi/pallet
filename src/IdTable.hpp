#pragma once

#include <algorithm>
#include <array>
#include <inttypes.h>
#include <vector>
#include "StaticVector.hpp"

namespace pallet::containers {

template <class EntryType, template<class> class Container = std::vector, class id_type = uint32_t>
class IdTable {
  struct Space {
    alignas(EntryType) std::byte bytes[sizeof(EntryType)];
    EntryType* ptr() {
      return reinterpret_cast<EntryType*>(this);
    }
    const EntryType* ptr() const {
      return reinterpret_cast<const EntryType*>(this);
    }
    template<class... Args>
    void construct(Args ...args) {
      new (this) EntryType(std::forward<Args>(args)...);
    }

    EntryType& ref() {
      return *(this->ptr());
    }
    
    const EntryType& ref() const {
      return *(this->ptr());
    }
    
    void destroy() {
      this->ptr()->~EntryType();
    }
  };
  Container<Space> storage;
  Container<id_type> freeVector;
public:

  IdTable() {}

  void reserve(size_t n) {
    storage.reserve(n);
    freeVector.reserve(n);
  }

  template <class T>
  id_type push(T&& item) {
    if (freeVector.size() != 0) {
      id_type index = freeVector.back();
      freeVector.pop_back();
      this->storage[index].construct(std::forward<T>(item));
      return index;
    }
    Space& space = storage.emplace_back();
    space.construct(std::forward<T>(item));
    return storage.size() - 1;
  }

  template <class... Args>
  id_type emplace(Args... args) {
    if (freeVector.size() != 0) {
      id_type index = freeVector.back();
      freeVector.pop_back();
      this->storage[index].construct(std::forward<Args>(args)...);
      return index;
    }
    Space& space = storage.emplace_back();
    space.construct(std::forward<Args>(args)...);
    return storage.size() - 1;
  }

  void free(id_type index) {
    Space& space = storage[index];
    space.destroy();
    freeVector.push_back(index);
  }

  EntryType& operator[](id_type index) {
    return storage[index].ref();
  }

  const EntryType& operator[](id_type index) const {
    return storage[index].ref();
  }

  ~IdTable() {
    std::sort(freeVector.begin(), freeVector.end(), std::greater<id_type>{});
    for (size_t idTableIndex = 0;
         idTableIndex < this->storage.size(); idTableIndex++) {
      if (freeVector.size() != 0 && freeVector.back() == idTableIndex) {
        freeVector.pop_back();
      } else {
        this->storage[idTableIndex].destroy();
      }
    }
  }
};
}
