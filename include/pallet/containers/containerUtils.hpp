#pragma once

// #include <new>

namespace pallet::containers {

  template <class ItemType>
  struct Space {
    alignas(ItemType) std::byte bytes[sizeof(ItemType)];
    ItemType* ptr() {
      return (reinterpret_cast<ItemType*>(&bytes[0]));
    }
    const ItemType* ptr() const {
      return (reinterpret_cast<const ItemType*>(&bytes[0]));
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
}
