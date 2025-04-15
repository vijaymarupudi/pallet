#pragma once
#include <memory>
#include <concepts>
#include <type_traits>
#include <utility>
#include <cstddef>
#include "pallet/LightVariant.hpp"

namespace pallet {

namespace detail {

template <class T>
class AbstractCallable;

template <class R, class... A>
class AbstractCallable<R(A...)> {
public:
  virtual R operator()(A...) = 0;
  virtual ~AbstractCallable() {};
};


template <class T, class FuncType>
class Invokable;

template <class T, class R, class... A>
class Invokable<T, R(A...)> : public T, public AbstractCallable<R(A...)> {
public:
  Invokable(auto&& item) : T{std::forward<decltype(item)>(item)} {}
  virtual R operator()(A... args) override {
    return T::operator()(args...);
  }
  virtual ~Invokable() override = default;
};
}

template <class T, class V = void>
struct CallablePropertiesStruct;

template <class T>
struct CallablePropertiesStruct<T> : public CallablePropertiesStruct<decltype(&T::operator())> {};

template <class T, class R, class... A>
struct CallablePropertiesStruct<R(T::*)(A...)> {
  using functionType = R(A...);
  using returnType = R;
};

template <class T, class R, class... A>
struct CallablePropertiesStruct<R(T::*)(A...) const> {
  using functionType = R(A...);
  using returnType = R;
};

template <class R, class... A>
struct CallablePropertiesStruct<R(*)(A...)> {
  using functionType = R(A...);
  using returnType = R;
};

template <class T>
using CallableFunctionType = CallablePropertiesStruct<T>::functionType;


template <class T>
class Callable;

template <class R, class... A>
class Callable<R(A...)> {
  using FuncPtrType = R(*)(A..., void*);
  using FuncPtrUdPair = std::pair<FuncPtrType, void*>;
  using AbsCallablePtr = std::unique_ptr<detail::AbstractCallable<R(A...)>>;

  pallet::LightVariant<FuncPtrUdPair, AbsCallablePtr> mfunc;
  
public:
  Callable(FuncPtrType funcPtr, void* ud) : mfunc{FuncPtrUdPair{funcPtr, ud}} {}

  template <class F>
  requires (
    std::invocable<F, A...> &&
    std::same_as<std::invoke_result_t<F, A...>, R> &&
    sizeof(F) <= sizeof(void*) &&
    alignof(F) <= alignof(void*) &&
    std::is_trivially_destructible_v<F> &&
    std::is_trivially_move_constructible_v<F>
    )
  
  Callable(F&& obj) : mfunc{FuncPtrUdPair{nullptr, nullptr}} {
    auto& funcPtrUdPair = *pallet::get_if<0>(mfunc);
    void*& ud = std::get<1>(funcPtrUdPair);
    new (&ud) F (std::forward<F>(obj));
    std::get<0>(funcPtrUdPair) = +[](A... args, void* ud) {
      return reinterpret_cast<F*>(&ud)->operator()(args...);
    };
  }

  template <class F>
  requires (
    std::invocable<F, A...> &&
    std::same_as<std::invoke_result_t<F, A...>, R>
    )
  Callable(F&& obj) :
    mfunc{AbsCallablePtr{std::make_unique<detail::Invokable<F, R(A...)>>(std::forward<F>(obj))}} {}

  Callable(std::nullptr_t) :
    mfunc{FuncPtrUdPair(nullptr, nullptr)} {}

  operator bool () {
    if (mfunc.index() == 0 && std::get<0>(pallet::get<0>(mfunc)) == nullptr) {
      return false;
    } else {
      return true;
    }
  }

  R operator()(A... args) {
    return pallet::visit(pallet::overloads {
        [&](FuncPtrUdPair& item) {
          auto& [cb, ub] = item;
          return cb(args..., ub);
        },
          [&](AbsCallablePtr& item) {
            return (*item)(args...);
          }
          }, mfunc);
  }
};

template <class T>
Callable(T&& arg) -> Callable<CallableFunctionType<T>>;

}
