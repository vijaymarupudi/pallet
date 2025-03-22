#pragma once

#include <utility>

namespace pallet {

template <class ...FunctionArgs>
struct CStyleCallback {
  using FunctionType = void(*)(FunctionArgs..., void*);
  FunctionType ptr = nullptr;
  void* data = nullptr;
  void set(FunctionType ptr, void* data) {
    this->ptr = ptr;
    this->data = data;
  }

  template <class ...CallArguments>
  void call(CallArguments... args) {
    if (this->ptr) {
      this->ptr(std::forward<CallArguments>(args)..., this->data);  
    }
  }
};

template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };

}

