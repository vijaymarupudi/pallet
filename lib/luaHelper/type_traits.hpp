#pragma once

namespace pallet::luaHelper {


template <auto data>
struct constant_wrapper {
  using value_type = decltype(data);
  static constexpr value_type value = data;
  constexpr operator value_type() const {
    return data;
  }
};

template <auto value>
constexpr constant_wrapper<value> cw{};

namespace detail {

template <class T>
struct FunctionTypeHelper;

template <class ReturnType,
          class CallableType,
          class... ArgumentType>
struct FunctionTypeHelper<ReturnType(CallableType::*)(ArgumentType...)> {
  using functionType = ReturnType(ArgumentType...);
};

// Redirect const operator() to the non-const version
template <class ReturnType, class CallableType, class... ArgumentType>
struct FunctionTypeHelper<ReturnType(CallableType::*)(ArgumentType...) const> : public FunctionTypeHelper<ReturnType(CallableType::*)(ArgumentType...)> {};

// Callable (operator()) support
template <class T>
requires requires {
  &T::operator();
}
struct FunctionTypeHelper<T> : public FunctionTypeHelper<decltype(&T::operator())> {};

// Function pointer support
template <class ReturnType, class... ArgumentType, ReturnType(*funcPtr)(ArgumentType...)>
struct FunctionTypeHelper<constant_wrapper<funcPtr>> : public FunctionTypeHelper<decltype([](ArgumentType... args) {
  return funcPtr(std::forward<ArgumentType>(args)...);
 })> {};
}

template <class T>
using FunctionType = detail::FunctionTypeHelper<std::decay_t<T>>::functionType;

struct PrivateTagType {};

constexpr PrivateTagType PrivateTag;

}


