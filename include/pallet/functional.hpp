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
    requires (std::invocable<F, A...>)
    Callable(F&& obj) : data{
        std::make_unique<detail::Invokable<F, R, A...>>(std::forward<F>(obj))
      } {}
    
    R operator()(A... args) {
      return visit(overloads {
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
