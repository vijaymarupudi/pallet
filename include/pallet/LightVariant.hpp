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
union VariadicUnion {
  Type var;
  VariadicUnion<Types...> others;
  constexpr inline VariadicUnion() {}
  constexpr inline ~VariadicUnion() {}
};

template <class Type>
union VariadicUnion<Type> {
  Type var;
  constexpr inline VariadicUnion() {}
  constexpr inline ~VariadicUnion() {}
};

template <size_t i, class... Types>
constexpr auto get_pointer_to_union_index(VariadicUnion<Types...>& arg) {
  if constexpr (i == 0) {
    return &arg.var;
  } else {
    return get_pointer_to_union_index<i - 1>(arg.others);
  }
}

template <size_t i, class... Types>
constexpr auto get_pointer_to_union_index(const VariadicUnion<Types...>& arg) {
  if constexpr (i == 0) {
    return &arg.var;
  } else {
    return get_pointer_to_union_index<i - 1>(arg.others);
  }
}

template <size_t i, class... Types>
constexpr decltype(auto) get_ref_to_union_index(VariadicUnion<Types...>& arg) {
  return *get_pointer_to_union_index<i>(arg);
}

template <size_t i, class... Types>
constexpr decltype(auto) get_ref_to_union_index(const VariadicUnion<Types...>& arg) {
  return *get_pointer_to_union_index<i>(arg);
}

}

using namespace detail;

template <class... Types>
class LightVariant {

  VariadicUnion<Types...> storage;
  unsigned char type;

public:

  static const size_t nTypes = sizeof...(Types);

  template <class Type, class... Args>
  requires std::disjunction_v<std::is_same<Type, Types>...>
  constexpr LightVariant(std::in_place_type_t<Type>, Args&&... args) {
    constexpr const size_t typeIndex = pallet::IndexOfTypeInVariadic<Type, Types...>;
    type = typeIndex;
    new (get_pointer_to_union_index<typeIndex>(storage)) Type (std::forward<Args>(args)...);
  }

  template <size_t typeIndex, class... Args>
  requires (typeIndex < sizeof...(Types))
  constexpr LightVariant(std::in_place_index_t<typeIndex>, Args&&... args) {
    using Type = pallet::IndexVariadic<typeIndex, Types...>;
    type = typeIndex;
    new (get_pointer_to_union_index<typeIndex>(storage)) Type (std::forward<Args>(args)...);
  }

  template <class T>
  requires std::disjunction_v<std::is_same<std::remove_cvref_t<T>, Types>...>
  constexpr LightVariant(T&& arg) {
    constexpr const size_t typeIndex = pallet::IndexOfTypeInVariadic<std::remove_cvref_t<T>, Types...>;
    type = typeIndex;
    new (get_pointer_to_union_index<typeIndex>(storage)) std::remove_cvref_t<T> (std::forward<T>(arg));
  }


  // move constructor
  constexpr LightVariant(LightVariant&& other)
    noexcept(std::conjunction_v<std::is_nothrow_move_constructible<Types>...>)
    requires (std::conjunction_v<std::is_move_constructible<Types>...>)
  : type(other.type) {
    takeActionRuntimeIdx<Types...>(this->type, [&]<class Type, size_t i>() {
        new (get_pointer_to_union_index<i>(storage)) Type (std::move(get_ref_to_union_index<i>(other.storage)));
      });
  }

  // copy constructor
  constexpr LightVariant(const LightVariant& other)
    noexcept(std::conjunction_v<std::is_nothrow_copy_constructible<Types>...>)
    requires (std::conjunction_v<std::is_copy_constructible<Types>...>)
  : type(other.type) {
    takeActionRuntimeIdx<Types...>(this->type, [&]<class Type, size_t i>() {
        new (get_pointer_to_union_index<i>(storage)) Type (get_ref_to_union_index<i>(other.storage));
      });
  }

  template <class... Args>
  constexpr LightVariant(Args&&... args)
  // don't use this for the copy constructor with a non-const argument
    requires (!(sizeof...(args) == 1 &&
                std::is_same_v<std::remove_cvref_t<Args>...,
                LightVariant>))
  {

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
          new (get_pointer_to_union_index<i>(storage)) Type (std::forward<Args>(args)...);
        }
      });
  }

  constexpr LightVariant& operator=(const LightVariant& other)
    noexcept(std::conjunction_v<std::conjunction<std::is_nothrow_copy_constructible<Types>, std::is_nothrow_copy_assignable<Types>>...>)
    requires (std::conjunction_v<std::conjunction<std::is_copy_constructible<Types>, std::is_copy_assignable<Types>>...>)
  {
    if (type == other.type) {
      takeActionRuntimeIdx<Types...>(type, [&]<class Type, size_t i>() {
          get_ref_to_union_index<i>(storage) = get_ref_to_union_index<i>(other.storage);
        });
    } else {
      takeActionRuntimeIdx<Types...>(type, [&]<class ThisType, size_t thisI>() {
          takeActionRuntimeIdx<Types...>(other.type, [&]<class OtherType, size_t otherI>() {
              get_ref_to_union_index<thisI>(storage).~ThisType();
              this->type = other.type;
              new (get_pointer_to_union_index<otherI>(storage)) OtherType (get_ref_to_union_index<otherI>(other.storage));
            });
        });
    }
    return *this;
  }

  constexpr LightVariant& operator=(LightVariant&& other)
    noexcept(std::conjunction_v<std::conjunction<std::is_nothrow_move_constructible<Types>, std::is_nothrow_move_assignable<Types>>...>)
    requires (std::conjunction_v<std::conjunction<std::is_move_constructible<Types>, std::is_move_assignable<Types>>...>)
  {
    if (type == other.type) {
      takeActionRuntimeIdx<Types...>(type, [&]<class Type, size_t i>() {
          std::swap(get_ref_to_union_index<i>(storage), get_ref_to_union_index<i>(other.storage));
        });
    } else {
      takeActionRuntimeIdx<Types...>(type, [&]<class ThisType, size_t thisI>() {
          takeActionRuntimeIdx<Types...>(other.type, [&]<class OtherType, size_t otherI>() {

              // move other's contents to tmp variables
              auto&& otherTypeStorage = get_ref_to_union_index<otherI>(other.storage);
              auto val = std::move(otherTypeStorage);
              auto otherType = other.type;

              // destroy the shell of other
              otherTypeStorage.~OtherType();

              // move the contents of this to other
              other.type = this->type;
              new (get_pointer_to_union_index<thisI>(other.storage)) ThisType (std::move(get_ref_to_union_index<thisI>(storage)));

              // destroy the shell of this
              get_ref_to_union_index<thisI>(storage).~ThisType();

              // move the temporaries into this
              this->type = otherType;
              new (get_pointer_to_union_index<otherI>(storage)) OtherType (std::move(val));
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
          auto&& ref = std::move(get_ref_to_union_index<i>(self.storage));
          return lambda(std::move(ref));
        } else {
          auto&& ref = get_ref_to_union_index<i>(self.storage);
          return lambda(ref);
        }
      });
  }

  template <class T>
  constexpr auto get_if(this auto&& self) {
    constexpr size_t i = pallet::IndexOfTypeInVariadic<T, Types...>;
    using ReturnType = decltype(get_pointer_to_union_index<i>(self.storage));
    if (self.type == i) { return get_pointer_to_union_index<i>(self.storage); }
    else { return ReturnType{nullptr}; }
  }

  template <size_t i>
  constexpr auto get_if(this auto&& self) {
    using ReturnType = decltype(get_pointer_to_union_index<i>(self.storage));
    if (self.type == i) { return get_pointer_to_union_index<i>(self.storage); }
    else { return ReturnType{nullptr}; }
  }


  template <class T>
  constexpr decltype(auto) get_unchecked(this auto&& self) {
    return *self.template get_if<T>();
  }

  constexpr ~LightVariant() {
    takeActionRuntimeIdx<Types...>(type, [&]<class Type, size_t i>() -> decltype(auto) {
        auto&& ref = get_ref_to_union_index<i>(this->storage);
        ref.~Type();
      });
  }
};


/*
 * LightVariant concept
 */
namespace detail {
template <class T>
struct IsLightVariant : std::false_type {};

template <class... Args>
struct IsLightVariant<LightVariant<Args...>> : std::true_type {};

}

namespace concepts {
template <class T>
concept LightVariant = detail::IsLightVariant<T>::value;
}

namespace detail {
template <class V>
struct LightVariantForEach {};

template <class... Types, size_t... indicies>
void lightVariantForEachApplication(auto&& lambda, std::index_sequence<indicies...>) {
  (lambda.template operator()<Types, indicies>(), ...);
}

template <class... Types>
struct LightVariantForEach<LightVariant<Types...>> {
  static void apply(auto&& lambda) {
    lightVariantForEachApplication<Types...>(std::forward<decltype(lambda)>(lambda),
                                 std::index_sequence_for<Types...>{});
  }
};
}

template <class V>
requires concepts::LightVariant<V>
void lightVariantForEach(auto&& lambda) {
  detail::LightVariantForEach<V>::apply(std::forward<decltype(lambda)>(lambda));
}


template <class T>
decltype(auto) visit(auto&& visitor, T&& variant) requires concepts::LightVariant<std::remove_cvref_t<T>>{
  return std::forward<T>(variant).visit(std::forward<decltype(visitor)>(visitor));
}

template <size_t i>
decltype(auto) get(auto&& item) {
  return *std::forward<decltype(item)>(item).template get_if<i>();
}

template <size_t i>
auto get_if(auto&& item) {
  return item.template get_if<i>();
}

}
