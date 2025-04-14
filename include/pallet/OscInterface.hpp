#pragma once

#include <string_view>
#include <cstdint>

#include "pallet/LightVariant.hpp"
#include "pallet/Event.hpp"


namespace pallet {

using OscItem = LightVariant<std::string_view, int32_t, float>;

class OscInterface {
public:
  using MessageEvent = Event<void(const char*, const OscItem*, size_t)>;

  using AddressId = size_t;
  using ServerId = size_t;

  virtual AddressId createAddress(int port) = 0;
  virtual void freeAddress(AddressId) = 0;

  virtual ServerId createServer(int port) = 0;
  virtual void freeServer(ServerId) = 0;

  virtual MessageEvent::Id listen(ServerId, MessageEvent::Callback) = 0;
  virtual void unlisten(ServerId, MessageEvent::Id) = 0;

  void sendMessage(const AddressId address, const char* path, const OscItem* items, size_t n);

  template <class... Types>
  void send(const AddressId address, const char* path, Types&&... args);

private:
  virtual void sendMessageImpl(const AddressId address,
                               const char* path,
                               const OscItem* items, size_t n) = 0;
};



/*
 * Private code
 */

inline void OscInterface::sendMessage(const AddressId address, const char* path, const OscItem* items, size_t n) {
  sendMessageImpl(address, path, items, n);
}

template <class... Types>
void OscInterface::send(const AddressId address, const char* path, Types&&... args) {
  if constexpr ((sizeof...(Types)) > 0) {
    OscItem items[] = {OscItem(std::forward<Types>(args))...};
    this->sendMessage(address, path, items, sizeof...(Types));
  } else {
    this->sendMessage(address, path, nullptr, 0);
  }
}


}
