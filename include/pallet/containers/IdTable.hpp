#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <concepts>
#include "containerUtils.hpp"
#include "StaticVector.hpp"

namespace pallet::containers {

namespace detail {
template <class T>
concept hasCapacity = requires(T& t) {
  { t.capacity() } -> std::convertible_to<size_t>;
};

template <class T>
concept hasReserve = requires (T& t, size_t n) {
  t.reserve(n);
};

template <class T>
concept Vectorish = hasReserve<T> && hasCapacity<T>;

template <class T, class ObjectType>
concept IdTableContainerRequirements = requires(T& t, size_t n) {
  t.pop_back();
  { t.size() } -> std::convertible_to<size_t>;
  t.emplace_back();
  { t.back() } -> std::same_as<ObjectType&>;
  { t[n] } -> std::same_as<ObjectType&>;
  t.clear();
};

template <class EntryType, class Container>
void recapContainer(Container& existing, size_t newCapacity) {
  auto newStorage = Container();
  newStorage.reserve(newCapacity);
  for (size_t i = 0; i < existing.size(); i++) {
    Space<EntryType>& space = newStorage.emplace_back();
    space.construct(std::move(existing[i].ref()));
    existing[i].ref().~EntryType();
  }
  existing = std::move(newStorage);
}
}


template <class IdTableType>
struct IdTableIterator;

template <class EntryType,
          template<class> class Container = std::vector,
          class IdType = uint32_t>
requires detail::IdTableContainerRequirements<Container<IdType>, IdType> &&
  detail::IdTableContainerRequirements<Container<EntryType>, EntryType> &&
  std::unsigned_integral<IdType>
class IdTable {
  friend struct IdTableIterator<IdTable>;
  using value_type = EntryType;
  using Id = IdType;
public:

  IdTable() {}

  void reserve(size_t n) {
    if constexpr (detail::Vectorish<Container<Space<EntryType>>>) {
      if (n <= storage.size()) { return; }
      detail::recapContainer<EntryType>(storage, n);
      freeVector.reserve(n);
    }
  }

  size_t size() {
    return storage.size() - freeVector.size();
  }

  size_t size() const {
    return storage.size() - freeVector.size();
  }

  template <class T>
  Id push(T&& item) {
    return emplace(std::forward<T>(item));
  }

  template <class... Args>
  Id emplace(Args... args) {
    if (freeVector.size() != 0) {
      Id index = freeVector.back();
      freeVector.pop_back();
      this->storage[index].construct(std::forward<Args>(args)...);
      return index;
    }
    return emplaceToStorage(std::forward<Args>(args)...);
  }

  void free(Id index) {
    Space<EntryType>& space = storage[index];
    space.destroy();
    freeVector.emplace_back(index);
  }

  EntryType& operator[](Id index) {
    return storage[index].ref();
  }

  const EntryType& operator[](Id index) const {
    return storage[index].ref();
  }

  IdTable(IdTable&& other) {

    std::sort(other.freeVector.begin(), other.freeVector.end(), std::greater<Id>{});

    for (size_t idTableStorageIndex = 0;
         idTableStorageIndex < other.storage.size();
         idTableStorageIndex++) {

      auto& space = this->storage.emplace_back();

      if (other.freeVector.size() != 0 && other.freeVector.back() == idTableStorageIndex) {
        // empty
        this->freeVector.emplace_back(idTableStorageIndex);
        other.freeVector.pop_back();
      } else {
        // not empty
        EntryType& entry = other.storage[idTableStorageIndex].ref();
        space.construct(std::move(entry));
      }
    }

    other.storage.clear();
    // other.freeVector.clear() not necessary because it should already be clear

  }

  IdTable& operator=(IdTable&& other) {
    std::swap(storage, other.storage);
    std::swap(freeVector, other.freeVector);
    return *this;
  }

  ~IdTable() {
    std::sort(freeVector.begin(), freeVector.end(), std::greater<Id>{});
    for (size_t idTableStorageIndex = 0;
         idTableStorageIndex < this->storage.size(); idTableStorageIndex++) {
      if (freeVector.size() != 0 && freeVector.back() == idTableStorageIndex) {
        freeVector.pop_back();
      } else {
        // not empty
        this->storage[idTableStorageIndex].destroy();
      }
    }
  }

  IdTableIterator<IdTable> begin () {
    std::sort(freeVector.begin(), freeVector.end(), std::greater<Id>{});
    return IdTableIterator<IdTable> {this, 0, static_cast<std::ptrdiff_t>(freeVector.size() - 1)};
  }

  IdTableIterator<IdTable> end () {
    return IdTableIterator<IdTable> {this, this->storage.size(),
                                     static_cast<std::ptrdiff_t>(freeVector.size() - 1)};
  }


private:
  Container<Space<EntryType>> storage;
  Container<Id> freeVector;

  template <class... Args>
  Id emplaceToStorage(Args&&... args) {
    if constexpr (detail::Vectorish<Container<Space<EntryType>>>) {
      bool hasCapacity = storage.capacity() >= storage.size() + 1;
      if (!hasCapacity) {
        size_t newCapacity = storage.size() + storage.size() / 2;
        this->reserve(newCapacity);
      }
    }
    Space<EntryType>& space = storage.emplace_back();
    space.construct(std::forward<Args>(args)...);
    return storage.size() - 1;
  }

};

template <class IdTableType>
struct IdTableIterator {

  IdTableType* table;
  std::size_t idTableStorageIndex;
  std::ptrdiff_t freeVectorIndex;

public:

  IdTableType::value_type& operator*() {
    return (*table)[idTableStorageIndex];
  }

  bool operator==(const IdTableIterator& other) const {
    return idTableStorageIndex == other.idTableStorageIndex;
  }

  IdTableIterator& operator++() {
    while (true) {
      idTableStorageIndex++;
      if (idTableStorageIndex >= table->storage.size()) {
        break;
      }
      else if (freeVectorIndex >= 0 && idTableStorageIndex == table->freeVector[freeVectorIndex]) {
        freeVectorIndex--;
      } else {
        break;
      }
    }
    return *this;
  }
};

}
