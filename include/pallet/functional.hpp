#pragma once
#include <memory>
#include <concepts>
#include "pallet/variant.hpp"

namespace pallet {

  namespace detail {
    template <class R, class... A>
    class AbstractCallable {
    public:
      virtual R operator()(A...) = 0;
      virtual ~AbstractCallable() {};
    };

    template <class T, class R, class... A>
    class Invokable : public T, public AbstractCallable<R, A...> {
    public:
      Invokable(auto&& item) : T{std::forward<decltype(item)>(item)} {}
      virtual R operator()(A... args) override {
        return T::operator()(args...);
      }
      virtual ~Invokable() override = default;
    };
  }

  template <class R, class... A>
  class Callable {
    using FuncPtrType = R(*)(A..., void*);
    using FuncPtr = std::pair<FuncPtrType, void*>;
    using AbsCallablePtr = std::unique_ptr<detail::AbstractCallable<R, A...>>;
    Variant<FuncPtr, AbsCallablePtr> data;

  public:
    Callable(FuncPtrType ptr, void* ud) : data{std::in_place_type_t<FuncPtr>{}, ptr, ud} {}

    template <class F>
    requires (
      std::invocable<F, A...> &&
      std::same_as<decltype(std::declval<F>()(std::declval<A>()...)), R> &&
      sizeof(F) <= sizeof(void*) &&
      alignof(F) <= alignof(void*) &&
      std::is_trivially_destructible_v<F> &&
      std::is_trivially_move_constructible_v<F>
      )
    Callable(F&& obj) {
      FuncPtr& pair = pallet::get_unchecked<FuncPtr>(data);
      void*& ud = std::get<1>(pair);
      new (&ud) F (std::forward<F>(obj));
      std::get<0>(pair) = +[](A... args, void* ud) {
        return reinterpret_cast<F*>(&ud)->operator()(args...);
      };
    }


    template <class F>
    requires (
      std::invocable<F, A...> &&
      std::same_as<decltype(std::declval<F>()(std::declval<A>()...)), R>
      )
    Callable(F&& obj) : data{std::make_unique<detail::Invokable<F, R, A...>>(std::forward<F>(obj))} {}
    
    R operator()(A... args) {
      return pallet::visit(pallet::overloads {
          [&](FuncPtr& funcPtr) -> R {
            auto& [cb, ud] = funcPtr;
            return cb(args..., ud);
          },
            [&](AbsCallablePtr& callable) -> R {
              return (*callable)(args...);
            }}, data);
    }
  };
}
