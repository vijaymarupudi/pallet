#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
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

  // TODO: items that have move constructors won't work properly because it won't be called when the Container is resizing

template <class EntryType, template<class> class Container = std::vector, class id_type = uint32_t>
requires detail::IdTableContainerRequirements<Container<id_type>, id_type> &&
  detail::IdTableContainerRequirements<Container<EntryType>, EntryType> &&
  std::unsigned_integral<id_type>
class IdTable {
public:

  IdTable() {}

  void reserve(size_t n) {
    if constexpr (detail::Vectorish<Container<Space<EntryType>>>) {
      if (n <= storage.size()) { return; }
      detail::recapContainer<EntryType>(storage, n);
      freeVector.reserve(n);
    }
  }

  template <class T>
  id_type push(T&& item) {
    return emplace(std::forward<T>(item));
  }

  template <class... Args>
  id_type emplace(Args... args) {
    if (freeVector.size() != 0) {
      id_type index = freeVector.back();
      freeVector.pop_back();
      this->storage[index].construct(std::forward<Args>(args)...);
      return index;
    }
    return emplaceToStorage(std::forward<Args>(args)...);
  }

  void free(id_type index) {
    Space<EntryType>& space = storage[index];
    space.destroy();
    freeVector.push_back(index);
  }

  EntryType& operator[](id_type index) {
    return storage[index].ref();
  }

  const EntryType& operator[](id_type index) const {
    return storage[index].ref();
  }

  IdTable(IdTable&& other) {

    std::sort(other.freeVector.begin(), other.freeVector.end(), std::greater<id_type>{});

    for (size_t idTableIndex = 0;
         idTableIndex < other.storage.size();
         idTableIndex++) {

      auto& space = this->storage.emplace_back();

      if (other.freeVector.size() != 0 && other.freeVector.back() == idTableIndex) {
        // empty
        this->freeVector.push_back(idTableIndex);
        other.freeVector.pop_back();
      } else {
        // not empty
        EntryType& entry = other.storage[idTableIndex].ref();
        space.construct(std::move(entry));
      }
    }

    other.storage.clear();
    // other.freeVector.clear() not necessary because it should already be clear

  }

  IdTable& operator=(IdTable&& other) {
    std::swap(storage, other.storage);
    std::swap(freeVector, other.freeVector);
  }

  ~IdTable() {
    std::sort(freeVector.begin(), freeVector.end(), std::greater<id_type>{});
    for (size_t idTableIndex = 0;
         idTableIndex < this->storage.size(); idTableIndex++) {
      if (freeVector.size() != 0 && freeVector.back() == idTableIndex) {
        freeVector.pop_back();
      } else {
        // not empty
        this->storage[idTableIndex].destroy();
      }
    }
  }

private:
  Container<Space<EntryType>> storage;
  Container<id_type> freeVector;

  template <class... Args>
  id_type emplaceToStorage(Args&&... args) {
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
}
