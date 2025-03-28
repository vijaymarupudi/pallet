#pragma once
#include <array>
#include <cstdint>
#include <stdio.h>
#include "containerUtils.hpp"

namespace pallet::containers {

template <class ItemType, size_t _capacity>
class StaticVector {
public:
  using size_type = size_t;
private:
  std::array<Space<ItemType>, _capacity> storage;
  size_type top;
public:
  StaticVector() : top(0) {}
  StaticVector(const StaticVector<ItemType, _capacity>& other) = delete;
  StaticVector(StaticVector<ItemType, _capacity>&& other) = delete;
  StaticVector<ItemType, _capacity>& operator=(const StaticVector<ItemType, _capacity>& other) = delete;
  StaticVector<ItemType, _capacity>& operator=(StaticVector<ItemType, _capacity>&& other) = delete;
  ~StaticVector() {
    for (size_type i = 0; i < this->size(); i++) {
      this->storage[i].destroy();
    }
  }

  size_type capacity() const {
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

  size_type size() const {
    return this->top;
  }

  ItemType& operator[](size_type index) {
    return this->storage[index].ref();
  }

  const ItemType& operator[](size_type index) const {
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

  void resize(size_type target) {
    if (target < this->size()) {
      for (size_type i = target; i < this->size(); i++) {
        this->storage[i].destroy();
      }
      this->top = target;
    } else if (target > this->size()) {
      for (size_type i = this->size(); i < target; i++) {
        this->storage[i].construct();
      }
      this->top = target;
    }
  }

  void reserve(size_type in) {
    (void)in;
    // do nothing
  }

};

}
