#pragma once
#include <cstddef>
#include <utility>
#include <type_traits>
#include <new>
#include "pallet/overloads.hpp"
#include "pallet/types.hpp"

namespace pallet {

namespace detail {

template <class... Types>
void variadicForEach(auto&& lambda) {
  auto mainLambda = [&]<size_t... i>(std::index_sequence<i...>) {
    (lambda.template operator()<pallet::IndexVariadic<i, Types...>, i>(), ...);
  };
  mainLambda(std::make_index_sequence<sizeof...(Types)>{});
}

template <class... Types>
constexpr decltype(auto) variadicTypeMap(auto&& lambda, auto&& then) {
  auto mainLambda = [&]<size_t... i>(std::index_sequence<i...>) constexpr -> decltype(auto) {
    return then.template operator()<([&]() constexpr -> decltype(auto) {
      using Type = pallet::IndexVariadic<i, Types...>;
      return lambda.template operator()<Type, i>();
    })()...>();
  };
  mainLambda(std::make_index_sequence<sizeof...(Types)>{});
}

template <size_t i, class Type, class... Others>
static inline decltype(auto) takeActionHelper(size_t runtimeIdx, auto&& lambda) {
  if constexpr (sizeof...(Others) == 0) {
    return lambda.template operator()<Type, i>();
  } else if (i == runtimeIdx) {
    return lambda.template operator()<Type, i>();
  } else {
    return takeActionHelper<i + 1, Others...>(runtimeIdx, std::forward<decltype(lambda)>(lambda));
  }
}

// runtime index
template <class... Types>
static inline decltype(auto) takeActionRuntimeIdx(size_t runtimeIdx, auto&& lambda) {
  return takeActionHelper<0, Types...>(runtimeIdx, std::forward<decltype(lambda)>(lambda));
}

template <class Type, class... Types>
struct VariantStorage {
  union {
    Type var;
    VariantStorage<Types...> others;
  };
  constexpr inline VariantStorage() {}
  constexpr inline ~VariantStorage() {}
};

template <class Type>
struct VariantStorage<Type> {
  union {
    Type var;
  };

  constexpr inline VariantStorage() {}
  constexpr inline ~VariantStorage() {}
};

template <size_t i, class... Types>
constexpr auto get_pointer_to_storage_index(VariantStorage<Types...>& arg) {
  if constexpr (i == 0) {
    return &arg.var;
  } else {
    return get_pointer_to_storage_index<i - 1>(arg.others);
  }
}

template <size_t i, class... Types>
constexpr auto get_pointer_to_storage_index(const VariantStorage<Types...>& arg) {
  if constexpr (i == 0) {
    return &arg.var;
  } else {
    return get_pointer_to_storage_index<i - 1>(arg.others);
  }
}

template <size_t i, class... Types>
constexpr decltype(auto) get_ref_to_storage_index(VariantStorage<Types...>& arg) {
  return *get_pointer_to_storage_index<i>(arg);
}

template <size_t i, class... Types>
constexpr decltype(auto) get_ref_to_storage_index(const VariantStorage<Types...>& arg) {
  return *get_pointer_to_storage_index<i>(arg);
}

}

using namespace detail;

template <class... Types>
class LightVariant {

  VariantStorage<Types...> storage;
  unsigned char type;

public:

  template <class Type, class... Args>
  requires std::disjunction_v<std::is_same<Type, Types>...>
  constexpr LightVariant(std::in_place_type_t<Type>, Args&&... args) {
    constexpr const size_t typeIndex = pallet::IndexOfTypeInVariadic<Type, Types...>;
    type = typeIndex;

    new (get_pointer_to_storage_index<typeIndex>(storage)) Type (std::forward<Args>(args)...);
  }

  template <class T>
  requires std::disjunction_v<std::is_same<T, Types>...>
  constexpr LightVariant(T&& arg) {
    constexpr const size_t typeIndex = pallet::IndexOfTypeInVariadic<T, Types...>;
    type = typeIndex;
    new (get_pointer_to_storage_index<typeIndex>(storage)) T (std::forward<T>(arg));
  }


  constexpr LightVariant(LightVariant&& other) : type(other.type) {
    takeActionRuntimeIdx<Types...>(this->type, [&]<class Type, size_t i>() {
        new (get_pointer_to_storage_index<i>(storage)) Type (std::move(get_ref_to_storage_index<i>(other.storage)));
      });
  }

  constexpr LightVariant(const LightVariant& other) : type(other.type) {
    takeActionRuntimeIdx<Types...>(this->type, [&]<class Type, size_t i>() {
        new (get_pointer_to_storage_index<i>(storage)) Type (get_ref_to_storage_index<i>(other.storage));
      });
  }

  constexpr LightVariant(LightVariant& other) : type(other.type) {
    takeActionRuntimeIdx<Types...>(this->type, [&]<class Type, size_t i>() {
        new (get_pointer_to_storage_index<i>(storage)) Type (get_ref_to_storage_index<i>(other.storage));
      });
  }

  template <class... Args>
  constexpr LightVariant(Args&&... args) {

    // Only one unique constructor?
    variadicTypeMap<Types...>([&]<class Type, size_t i>() constexpr {
        if constexpr (std::is_constructible_v<Type, Args...>)
          { return 1; }
        else { return 0; }
      }, [&]<int... results>() constexpr {
        static_assert((results + ...) == 1, "Multiple potential constructors");
      });

    // Then construct
    variadicForEach<Types...>([&]<class Type, size_t i>() {
        if constexpr (std::is_constructible_v<Type, Args...>) {
          type = i;
          new (get_pointer_to_storage_index<i>(storage)) Type (std::forward<Args>(args)...);
        }
      });
  }

  constexpr LightVariant& operator=(const LightVariant& other) {
    if (type == other.type) {
      takeActionRuntimeIdx<Types...>(type, [&]<class Type, size_t i>() {
          get_ref_to_storage_index<i>(storage) = get_ref_to_storage_index<i>(other.storage);
        });
    } else {
      takeActionRuntimeIdx<Types...>(type, [&]<class ThisType, size_t thisI>() {
          takeActionRuntimeIdx<Types...>(other.type, [&]<class OtherType, size_t otherI>() {
              get_ref_to_storage_index<thisI>(storage).~ThisType();
              this->type = other.type;
              new (get_pointer_to_storage_index<otherI>(storage)) OtherType (get_ref_to_storage_index<otherI>(other.storage));
            });
        });
    }
    return *this;
  }

  constexpr LightVariant& operator=(LightVariant&& other) {
    if (type == other.type) {
      takeActionRuntimeIdx<Types...>(type, [&]<class Type, size_t i>() {
          std::swap(get_ref_to_storage_index<i>(storage), get_ref_to_storage_index<i>(other.storage));
        });
    } else {
      takeActionRuntimeIdx<Types...>(type, [&]<class ThisType, size_t thisI>() {
          takeActionRuntimeIdx<Types...>(other.type, [&]<class OtherType, size_t otherI>() {

              // move other's contents to tmp variables
              auto&& otherTypeStorage = get_ref_to_storage_index<otherI>(other.storage);
              auto val = std::move(otherTypeStorage);
              auto otherType = other.type;

              // destroy the shell of other
              otherTypeStorage.~OtherType();

              // move the contents of this to other
              other.type = this->type;
              new (get_pointer_to_storage_index<thisI>(other.storage)) ThisType (std::move(get_ref_to_storage_index<thisI>(storage)));

              // destroy the shell of this
              get_ref_to_storage_index<thisI>(storage).~ThisType();

              // move the temporaries into this
              this->type = otherType;
              new (get_pointer_to_storage_index<otherI>(storage)) OtherType (std::move(val));
            });
        });
    }
    return *this;
  }

  constexpr size_t index() const {
    return type;
  }


  constexpr decltype(auto) visit(this auto&& self, auto&& lambda) {
    return takeActionRuntimeIdx<Types...>(self.type, [&]<class Type, size_t i>() -> decltype(auto) {
        if constexpr (std::is_rvalue_reference_v<decltype(self)>) {
          auto&& ref = std::move(get_ref_to_storage_index<i>(self.storage));
          return lambda(std::move(ref));
        } else {
          auto&& ref = get_ref_to_storage_index<i>(self.storage);
          return lambda(ref);
        }
      });
  }

  template <class T>
  constexpr auto get_if(this auto&& self) {
    constexpr size_t i = pallet::IndexOfTypeInVariadic<T, Types...>;
    using ReturnType = decltype(get_pointer_to_storage_index<i>(self.storage));
    if (self.type == i) { return get_pointer_to_storage_index<i>(self.storage); }
    else { return ReturnType{nullptr}; }
  }

  template <size_t i>
  constexpr auto get_if(this auto&& self) {
    using ReturnType = decltype(get_pointer_to_storage_index<i>(self.storage));
    if (self.type == i) { return get_pointer_to_storage_index<i>(self.storage); }
    else { return ReturnType{nullptr}; }
  }

  constexpr ~LightVariant() {
    takeActionRuntimeIdx<Types...>(type, [&]<class Type, size_t i>() -> decltype(auto) {
        auto&& ref = get_ref_to_storage_index<i>(this->storage);
        ref.~Type();
      });
  }
};
}
