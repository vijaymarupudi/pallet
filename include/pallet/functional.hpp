#pragma once
#include <memory>
#include <concepts>

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

    enum {
      TYPE_FUNC_PTR,
      TYPE_CALLABLE_PTR
    };

    unsigned char type;
    union {
      FuncPtr ptr;
      AbsCallablePtr callablePtr;
    };

    void moveConstructFromOther(Callable& other) {
      this->type = other.type;
      switch (type) {
      case TYPE_FUNC_PTR:
        new (&this->ptr) FuncPtr {std::move(other.ptr)};
        break;
      case TYPE_CALLABLE_PTR:
        new (&this->callablePtr) AbsCallablePtr {std::move(other.callablePtr)};
        break;
      }
    }

  public:
    Callable(FuncPtrType ptr, void* ud) : type(TYPE_FUNC_PTR),
                                          ptr{ptr, ud} {}

    template <class F>
    requires (
      std::invocable<F, A...> &&
      std::same_as<std::invoke_result_t<F, A...>, R> &&
      sizeof(F) <= sizeof(void*) &&
      alignof(F) <= alignof(void*) &&
      std::is_trivially_destructible_v<F> &&
      std::is_trivially_move_constructible_v<F>
      )

    Callable(F&& obj) : type(TYPE_FUNC_PTR), ptr{nullptr, nullptr} {
      void*& ud = std::get<1>(ptr);
      new (&ud) F (std::forward<F>(obj));
      std::get<0>(ptr) = +[](A... args, void* ud) {
        return reinterpret_cast<F*>(&ud)->operator()(args...);
      };
    }

    Callable(Callable&& other) {
      moveConstructFromOther(other);
    }

    Callable& operator=(Callable&& other) {
      if (other.type == this->type) {
        switch (type) {
        case TYPE_FUNC_PTR:
          std::swap(this->ptr, other.ptr);
          break;
        case TYPE_CALLABLE_PTR:
          std::swap(this->callablePtr, other.callablePtr);
          break;
        }
      } else {
        switch (other.type) {
        case TYPE_FUNC_PTR:
          {
            auto tmp = std::move(other.ptr);
            auto otherType = other.type;
            other.~Callable();
            other.moveConstructFromOther(*this);
            this->~Callable();
            this->type = otherType;
            new (&this->ptr) FuncPtr(std::move(tmp));
          }

          break;
        case TYPE_CALLABLE_PTR:
          {
            auto tmp = std::move(other.callablePtr);
            auto otherType = other.type;
            other.~Callable();
            other.moveConstructFromOther(*this);
            this->~Callable();
            this->type = otherType;
            new (&this->callablePtr) AbsCallablePtr(std::move(tmp));
          }
          break;
        }
      }
      return *this;
    }

    template <class F>
    requires (
      std::invocable<F, A...> &&
      std::same_as<std::invoke_result_t<F, A...>, R>
      )
    Callable(F&& obj) : type(TYPE_CALLABLE_PTR),
                        callablePtr{std::make_unique<detail::Invokable<F, R, A...>>(std::forward<F>(obj))} {}

    R operator()(A... args) {
      switch (type) {
      case TYPE_FUNC_PTR:
        {
          auto& [cb, ub] = this->ptr;
          return cb(args..., ub);
        }
      case TYPE_CALLABLE_PTR:
        return (*callablePtr)(args...);
      }
    }

    ~Callable() {
      switch (type) {
      case TYPE_FUNC_PTR:
        ptr.~FuncPtr();
        break;
      case TYPE_CALLABLE_PTR:
        callablePtr.~AbsCallablePtr();
      }
    }
  };
}
