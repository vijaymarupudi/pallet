#pragma once

#include <string_view>
#include <cstdint>

#include "variant.hpp"


namespace pallet {

using OscItem = Variant<std::string_view, int32_t, float>;

class OscInterface {

public:

  using AddressIdType = size_t;
  using OnMessageCallback = void(*)(const char* path, const OscItem*, size_t n, void*);

  int port = 0;

  void setOnMessage(OnMessageCallback cb, void* userData) {
    this->onMessageCb = cb;
    this->onMessageUserData = userData;
  }

  virtual AddressIdType createAddress(int port) = 0;
  virtual void freeAddress(AddressIdType) = 0;
  virtual void bind(int port) {
    this->port = port;
  };

  void sendMessage(const AddressIdType address, const char* path, const OscItem* items, size_t n) {
    sendMessageImpl(address, path, items, n);
  }

  template <class... Types>
  void send(const AddressIdType address, const char* path, Types&&... args) {
    if constexpr ((sizeof...(Types)) > 0) {
      OscItem items[] = {OscItem(std::forward<Types>(args))...};
      this->sendMessage(address, path, items, sizeof...(Types));
    } else {
      this->sendMessage(address, path, nullptr, 0);
    }
  }

protected:
  void uponMessage(const char* path, const OscItem* item, size_t n) {
    if (this->onMessageCb) {
      this->onMessageCb(path, item, n, this->onMessageUserData);
    }
  }

private:
  OnMessageCallback onMessageCb = nullptr;
  void* onMessageUserData = nullptr;
  virtual void sendMessageImpl(const AddressIdType address, const char* path, const OscItem* items, size_t n) = 0;
};

}
