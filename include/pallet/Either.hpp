#pragma once
#include <utility>
#include <type_traits>
#include <new>
#include "pallet/overloads.hpp"


namespace pallet {
template <class EType, class OType>
class Either {
  union {
    EType etype;
    OType otype;
  };
  bool type;

public:

  template <class T>
  requires std::is_same_v<T, EType>
  Either(T&& item) : etype(std::forward<T>(item)), type(false)  {}

  template <class T>
  requires std::is_same_v<T, OType>
  Either(T&& item) : otype(std::forward<T>(item)), type(true) {}

  template <class... Args>
  requires std::is_constructible_v<EType, Args...>
  Either(Args&&... args) : etype(std::forward<Args>(args)...), type(false) {}

  template <class... Args>
  requires std::is_constructible_v<OType, Args...>
  Either(Args&&... args) : otype(std::forward<Args>(args)...), type(true) {}

  Either(Either&& other) : type(other.type) {
    if (type) {
      new (&otype) OType (std::move(other.otype));
    } else {
      new (&etype) OType (std::move(other.etype)); 
    }
  }

  Either& operator=(Either&& other) {
    if (type == other.type) {
      if (type) {
        std::swap(otype, other.otype);
      } else {
        std::swap(etype, other.etype);
      }
    } else {
      if (other.type) {
        auto otherOtype = std::move(other.otype);
        bool otherType = other.type;
        other.~Either();
        new (&other.etype) EType (std::move(etype));
        this->~Either();
        this->type = otherType;
        new (&this->otype) OType (std::move(otherOtype));
      } else {
        auto otherEtype = std::move(other.etype);
        bool otherType = other.type;
        other.~Either();
        new (&other.otype) OType (std::move(otype));
        this->~Either();
        this->type = otherType;
        new (&this->etype) EType (std::move(otherEtype));
      }
    }
    return *this;
  }

  decltype(auto) visit(auto&& callable) {
    if (type) {
      return callable(otype);
    } else {
      return callable(etype);
    }
  }
  
  ~Either() {
    if (type) {
      otype.~OType();
    } else {
      etype.~EType();
    }
  }
  
};
}
