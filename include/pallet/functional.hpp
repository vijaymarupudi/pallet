#pragma once
#include <memory>
#include <concepts>
#include <type_traits>
#include <utility>

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

  enum class StorageType : unsigned char {
    FuncPtrUdPair,
    CallablePtr
  };

  StorageType type;
  
  union {
    FuncPtrUdPair funcPtrUdPair;
    AbsCallablePtr callablePtr;
  };

  void moveConstructFromOther(Callable& other) {
    this->type = other.type;
    switch (type) {
    case StorageType::FuncPtrUdPair:
      new (&this->funcPtrUdPair) FuncPtrUdPair {std::move(other.funcPtrUdPair)};
      break;
    case StorageType::CallablePtr:
      new (&this->callablePtr) AbsCallablePtr {std::move(other.callablePtr)};
      break;
    }
  }

public:
  Callable(FuncPtrType funcPtr, void* ud) : type(StorageType::FuncPtrUdPair),
                                            funcPtrUdPair{funcPtr, ud} {}

  template <class F>
  requires (
    std::invocable<F, A...> &&
    std::same_as<std::invoke_result_t<F, A...>, R> &&
    sizeof(F) <= sizeof(void*) &&
    alignof(F) <= alignof(void*) &&
    std::is_trivially_destructible_v<F> &&
    std::is_trivially_move_constructible_v<F>
    )

  Callable(F&& obj) : type(StorageType::FuncPtrUdPair), funcPtrUdPair{nullptr, nullptr} {
    void*& ud = std::get<1>(funcPtrUdPair);
    new (&ud) F (std::forward<F>(obj));
    std::get<0>(funcPtrUdPair) = +[](A... args, void* ud) {
      return reinterpret_cast<F*>(&ud)->operator()(args...);
    };
  }

  Callable(Callable&& other) {
    moveConstructFromOther(other);
  }

  Callable& operator=(Callable&& other) {
    if (other.type == this->type) {
      switch (type) {
      case StorageType::FuncPtrUdPair:
        std::swap(this->funcPtrUdPair, other.funcPtrUdPair);
        break;
      case StorageType::CallablePtr:
        std::swap(this->callablePtr, other.callablePtr);
        break;
      }
    } else {
      switch (other.type) {
      case StorageType::FuncPtrUdPair:
        {
          auto tmp = std::move(other.funcPtrUdPair);
          auto otherType = other.type;
          other.~Callable();
          other.moveConstructFromOther(*this);
          this->~Callable();
          this->type = otherType;
          new (&this->funcPtrUdPair) FuncPtrUdPair(std::move(tmp));
        }

        break;
      case StorageType::CallablePtr:
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
  Callable(F&& obj) : type(StorageType::CallablePtr),
                      callablePtr{std::make_unique<detail::Invokable<F, R(A...)>>(std::forward<F>(obj))} {}

  R operator()(A... args) {
    switch (type) {
    case StorageType::FuncPtrUdPair:
      {
        auto& [cb, ub] = this->funcPtrUdPair;
        return cb(args..., ub);
      }
      break;
    case StorageType::CallablePtr:
      return (*callablePtr)(args...);
    }

    std::unreachable();
  }

  ~Callable() {
    switch (type) {
    case StorageType::FuncPtrUdPair:
      funcPtrUdPair.~FuncPtrUdPair();
      break;
    case StorageType::CallablePtr:
      callablePtr.~AbsCallablePtr();
      break;
    }
  }
};

template <class T>
Callable(T&& arg) -> Callable<CallableFunctionType<T>>;

}
