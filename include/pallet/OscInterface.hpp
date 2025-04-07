#pragma once

#include <string_view>
#include <cstdint>

#include "pallet/variant.hpp"
#include "pallet/Event.hpp"


namespace pallet {

using OscItem = Variant<std::string_view, int32_t, float>;

class OscInterface {
public:
  using AddressId = size_t;
  Event<const char*, const OscItem*, size_t> onMessage;
  int port = 0;
  virtual AddressId createAddress(int port) = 0;
  virtual void freeAddress(AddressId) = 0;
  virtual void bind(int port);

  void sendMessage(const AddressId address, const char* path, const OscItem* items, size_t n);

  template <class... Types>
  void send(const AddressId address, const char* path, Types&&... args);

protected:
  void uponMessage(const char* path, const OscItem* item, size_t n);

private:
  virtual void sendMessageImpl(const AddressId address,
                               const char* path,
                               const OscItem* items, size_t n) = 0;
};



/*
 * Private code
 */

inline void OscInterface::bind(int port) {
  this->port = port;
};

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

inline void OscInterface::uponMessage(const char* path, const OscItem* item, size_t n) {
  this->onMessage.emit(path, item, n);
}


}
