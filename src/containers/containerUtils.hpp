#pragma once

namespace pallet::containers {
  
  template <class ItemType>
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
}
