#pragma once
#include <array>
#include <cstdint>
#include <stdio.h>
#include "containerUtils.hpp"
#include "pallet/math.hpp"

namespace pallet::containers {

template <class ItemType, size_t _capacity>
class StaticVector {
public:
  using SizeType = size_t;

private:
  std::array<Space<ItemType>, _capacity> storage;
  SizeType top;
public:
  StaticVector() : top(0) {}
  
  StaticVector(StaticVector&& other) {
    this->top = other.top;
    for (SizeType i = 0; i < other.size(); i++) {
      this->storage[i].construct(std::move(other.storage[i].ref()));
    }
    other.top = 0;
  }

  ~StaticVector() {
    for (SizeType i = 0; i < this->size(); i++) {
      this->storage[i].destroy();
    }
  }

  StaticVector& operator=(StaticVector&& other) {

    StaticVector* less;
    StaticVector* more;

    if (this->size() > other.size()) {
      more = this;
      less = &other;
    } else {
      less = this;
      more = &other;
    }

    for (SizeType i = 0; i < less->size(); i++) {
      std::swap(less->storage[i].ref(), more->storage[i].ref());
    }

    for (SizeType i = less->size(); i < more->size(); i++) {
      less->storage[i].construct(std::move(more->storage[i].ref()));
      more->storage[i].destroy();
    }

    std::swap(less->top, more->top);
    return *this;
  }

  StaticVector(const StaticVector& other) {
    this->top = other.top;
    for (SizeType i = 0; i < other.size(); i++) {
      this->storage[i].construct(other.storage[i].ref());
    }
  }

  StaticVector& operator=(const StaticVector& other) {
    this->clear();
    for (SizeType i = 0; i < other.size(); i++) {
      this->storage[i].construct(other.storage[i].ref());
    }
    this->top = other.top;
  }

  SizeType capacity() const {
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

  SizeType size() const {
    return this->top;
  }

  ItemType& operator[](SizeType index) {
    return this->storage[index].ref();
  }

  const ItemType& operator[](SizeType index) const {
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

  void resize(SizeType target) {
    if (target < this->size()) {
      for (SizeType i = target; i < this->size(); i++) {
        this->storage[i].destroy();
      }
      this->top = target;
    } else if (target > this->size()) {
      for (SizeType i = this->size(); i < target; i++) {
        this->storage[i].construct();
      }
      this->top = target;
    }
  }

  void clear() {
    this->resize(0);
  }

  void reserve(SizeType in) {
    (void)in;
    // do nothing
  }

};

}
