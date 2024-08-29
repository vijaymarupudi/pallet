#pragma once
#include <array>
#include <inttypes.h>
#include <stdio.h>

template <class ItemType, size_t _capacity>
class StaticVector {
  struct Space {
    alignas(ItemType) std::byte bytes[sizeof(ItemType)];
    ItemType* ptr() {
      return reinterpret_cast<ItemType*>(this);
    }
    const ItemType* ptr() const {
      return reinterpret_cast<const ItemType*>(this);
    }
    template<class... Args>
    void construct(Args ...args) {
      new (this) ItemType(std::forward<Args>(args)...);
    }

    ItemType& ref() {
      return *(this->ptr());
    }
    
    const ItemType& ref() const {
      return *(this->ptr());
    }
    
    void destroy() {
      this->ptr()->~ItemType();
    }
  };
  std::array<Space, _capacity> storage;
  size_t top;

public:
  StaticVector() : top(0) {}
  StaticVector(const StaticVector<ItemType, _capacity>& other) = delete;
  StaticVector(StaticVector<ItemType, _capacity>&& other) = delete;
  StaticVector<ItemType, _capacity>& operator=(const StaticVector<ItemType, _capacity>& other) = delete;
  StaticVector<ItemType, _capacity>& operator=(StaticVector<ItemType, _capacity>&& other) = delete;
  ~StaticVector() {
    for (size_t i = 0; i < this->size(); i++) {
      this->storage[i].destroy();
    }
  }

  size_t capacity() const {
    return _capacity;
  }

  template <class T>
  void push_back(T&& item) {
    this->storage[this->top].construct(std::forward<T>(item));
    this->top++;
    
  }
  template <class... Args>
  ItemType& emplace_back(Args... args) {
    storage[this->top].construct(std::forward<Args>(args)...);
    this->top++;
    return storage[this->top - 1].ref();
  }

  void pop_back() {
    storage[this->top - 1].destroy();
    this->top--;
  }

  ItemType& back() {
    return this->storage[this->size() - 1].ref();
  }

  const ItemType& back() const {
    return this->storage[this->size() - 1].ref();
  }

  size_t size() const {
    return this->top;
  }

  ItemType& operator[](size_t index) {
    return this->storage[index].ref();
  }

  const ItemType& operator[](size_t index) const {
    return this->storage[index].ref();
  }

  ItemType* begin() {
    return this->storage[0].ptr();
  }

  const ItemType* begin() const {
    return this->storage[0].ptr();
  }

  ItemType* end() {
    return this->storage[this->size()].ptr();
  }

  const ItemType* end() const {
    return this->storage[this->size()].ptr();
  }

  void resize(size_t target) {
    if (target < this->size()) {
      for (size_t i = target; i < this->size(); i++) {
        this->storage[i].destroy();
      }
      this->top = target;
    } else if (target > this->size()) {
      for (size_t i = this->size(); i < target; i++) {
        this->storage[i].construct();
      }
      this->top = target;
    }
  }

  void reserve(size_t in) {
    // do nothing
  }

};

// template <class EntryType, size_t _capacity>
// class IdTable {
//   struct Entry {
//     alignas(EntryType) std::byte bytes[sizeof(EntryType)];
//   };
//   std::array<Entry, _capacity> storage;
//   size_t top = 0;
//   template <class T>
//   size_t push(T&& val) {

//   }
// };
