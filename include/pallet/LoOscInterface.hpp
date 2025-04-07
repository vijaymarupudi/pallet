#pragma once

#include <type_traits>
#include <vector>
#include <memory>

#include "lo/lo.h"

#include "pallet/OscInterface.hpp"
#include "pallet/containers/IdTable.hpp"
#include "pallet/PosixPlatform.hpp"
#include "pallet/error.hpp"

namespace pallet {

static inline lo_message oscMessageToLoMessage(const OscItem* items, size_t n) {
  lo_message ret = lo_message_new();
  auto visitor = overloads {
    [&](const std::string_view& view) {
      lo_message_add_string(ret, view.data());
    },
    [&](const int32_t& number) {
      lo_message_add_int32(ret, number);
    },
    [&](const float& number) {
      lo_message_add_float(ret, number);
    }
  };

  for (size_t i = 0; i < n; i++) {
    pallet::visit(visitor, items[i]);
  }

  return ret;
}


static void oscInterfaceLoOnErrorFunc(int num, const char *msg, const char *path)
{
  printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

typedef void(*LoOscInterfaceCallback)(const char *path,
                                    const char *types,
                                    lo_arg ** argv,
                                    int argc, lo_message data,
                                    void *userData);

  namespace detail {
    struct LoAddressTypeCleanup {
      void operator()(lo_address addr) {
        lo_address_free(addr);
      }
    };

    using LoAddressType = std::unique_ptr<std::remove_pointer_t<lo_address>, LoAddressTypeCleanup>;

    inline LoAddressType makeLoAddress(int port) {
      char buf[16];
      snprintf(buf, 16, "%d", port);
      return LoAddressType {lo_address_new(NULL, buf)};
    }

    struct LoServerTypeCleanup {
      void operator()(lo_server server) {
        lo_server_free(server);
      }
    };

    using LoServerType = std::unique_ptr<std::remove_pointer_t<lo_server>, LoServerTypeCleanup>;

    inline LoServerType makeLoServer(int port) {
      char buf[16];
      snprintf(buf, 16, "%d", port);
      auto server = lo_server_new(buf, oscInterfaceLoOnErrorFunc);
      return LoServerType {server};
    }
  }

class LoOscInterface final : public OscInterface {
public:

  int port = 0;

  static Result<LoOscInterface> create(PosixPlatform& platform) {
    return Result<LoOscInterface>(std::in_place_t{}, platform);
  }

  LoOscInterface(PosixPlatform& platform) : platform(&platform) {}

  virtual AddressId createAddress(int port) override {
    auto addr = detail::makeLoAddress(port);
    auto id = addressIdTable.push(std::move(addr));
    return id;
  }

  virtual void freeAddress(AddressId id) override {
    addressIdTable.free(id);
  }

  void bind(int port) {
    this->server = detail::makeLoServer(port);
    this->port = port;
    lo_server_add_method(this->server.get(), NULL, NULL, LoOscInterface::loUponOscMessageCallback, this);
    int fd = lo_server_get_socket_fd(this->server.get());
    this->platform->watchFdIn(fd, LoOscInterface::loServerFdReadyCallback, this);
  }

  // void sendMessage(address_type addr, const char* path, const char* type, ...) {
  //   lo_message msg = lo_message_new();
  //   va_list args;
  //   va_start(args, type);
  //   lo_message_add_varargs(msg, type, args);
  //   this->sendMessage(addr, path, msg);
  //   lo_message_free(msg);
  // }

  ~LoOscInterface() {
    int fd = lo_server_get_socket_fd(this->server.get());
    this->platform->unwatchFdEvents(fd, PosixPlatform::Read);
  }

private:

  PosixPlatform* platform;
  detail::LoServerType server;
  pallet::containers::IdTable<detail::LoAddressType, std::vector, AddressId> addressIdTable;
  std::vector<OscItem> messageBuffer;

  void uponLoMessage(const char *path, const char *types,
                     lo_arg ** argv,
                     int argc, lo_message data) {
    (void)data;
    auto& buffer = this->messageBuffer;
    for (int i = 0; i < argc; i++) {
      auto arg = argv[i];
      switch (types[i]) {
      case 'i':
        buffer.emplace_back(arg->i32);
        break;
      case 's':
        buffer.emplace_back(&(arg->s));
        break;
      case 'f':
        buffer.emplace_back(arg->f);
        break;
      default:
        break;
      }
    }

    this->uponMessage(path, buffer.data(), buffer.size());
    buffer.clear();
  }

  virtual void sendMessageImpl(const AddressId address, const char* path,
                               const OscItem* items, size_t n) override {
    auto message = oscMessageToLoMessage(items, n);
    lo_send_message(addressIdTable[address].get(), path, message);
    lo_message_free(message);
  }


  static void loServerFdReadyCallback(int fd, short revents, void* userData) {
    (void)fd;
    (void)revents;
    auto& server = static_cast<LoOscInterface*>(userData)->server;
    lo_server_recv_noblock(server.get(), 0);
  }

  static int loUponOscMessageCallback(const char *path, const char *types,
                                      lo_arg ** argv,
                                      int argc, lo_message data,
                                      void *user_data) {
    static_cast<LoOscInterface*>(user_data)->uponLoMessage(path, types, argv, argc, data);
    return 0;
  }
};

}
