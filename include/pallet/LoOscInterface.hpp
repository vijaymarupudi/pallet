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


struct LoServer {
  LoServerType loServer;
  OscInterface::MessageEvent event;
  std::vector<OscItem> messageBuffer;
  PosixPlatform* platform;
  int port;

  LoServer(int port, PosixPlatform* iplatform) : loServer(makeLoServer(port)), platform(iplatform), port(port) {
    lo_server_add_method(this->loServer.get(), NULL, NULL, LoServer::uponLoOscMessageCallback, this);
    int fd = lo_server_get_socket_fd(this->loServer.get());
    this->platform->watchFdIn(fd, LoServer::loServerFdReadyCallback, this);
  }

  LoServer(LoServer&& other) :
    loServer(std::move(other.loServer)),
    event(std::move(other.event)),
    messageBuffer(std::move(other.messageBuffer)),
    platform(other.platform),
    port(other.port)
  {
    int fd = lo_server_get_socket_fd(this->loServer.get());
    this->platform->unwatchFdIn(fd);
    this->platform->watchFdIn(fd, LoServer::loServerFdReadyCallback, this);
  }

  LoServer& operator=(LoServer&& other) {
    std::swap(loServer, other.loServer);
    std::swap(event, other.event);
    std::swap(messageBuffer, other.messageBuffer);
    std::swap(platform, other.platform);
    std::swap(port, other.port);

    {
      int fd = lo_server_get_socket_fd(this->loServer.get());
      this->platform->unwatchFdIn(fd);
      this->platform->watchFdIn(fd, LoServer::loServerFdReadyCallback, this);
    }

    {
      int fd = lo_server_get_socket_fd(other.loServer.get());
      other.platform->unwatchFdIn(fd);
      other.platform->watchFdIn(fd, LoServer::loServerFdReadyCallback, &other);
    }
    
    return *this;
  }

  ~LoServer() {
    if (loServer) {
      int fd = lo_server_get_socket_fd(this->loServer.get());
      this->platform->unwatchFdIn(fd);
    }
  }

private:
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

    this->event.emit(path, buffer.data(), buffer.size());
    buffer.clear();
  }
  
  static int uponLoOscMessageCallback(const char *path, const char *types,
                                      lo_arg ** argv,
                                      int argc, lo_message data,
                                      void *user_data) {
    static_cast<LoServer*>(user_data)->uponLoMessage(path, types, argv, argc, data);
    return 0;
  }

  static void loServerFdReadyCallback(int fd, short revents, void* userData) {
    (void)fd;
    (void)revents;
    auto& loServer = static_cast<LoServer*>(userData)->loServer;
    lo_server_recv_noblock(loServer.get(), 0);
  }
};

}

class LoOscInterface final : public OscInterface {
public:

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

  virtual ServerId createServer(int port) override {
    return serverIdTable.emplace(port, platform);
  }

  virtual void freeServer(ServerId id) override {
    serverIdTable.free(id);
  }

  virtual MessageEvent::Id listen(ServerId id, MessageEvent::Callback callback) override {
    return serverIdTable[id].event.listen(std::move(callback));
  }
  
  virtual void unlisten(ServerId id, MessageEvent::Id eid) override {
    serverIdTable[id].event.unlisten(eid);
  }

private:

  PosixPlatform* platform;
  pallet::containers::IdTable<detail::LoAddressType, std::vector, AddressId> addressIdTable;
  pallet::containers::IdTable<detail::LoServer, std::vector, ServerId> serverIdTable;

  virtual void sendMessageImpl(const AddressId address, const char* path,
                               const OscItem* items, size_t n) override {
    auto message = oscMessageToLoMessage(items, n);
    lo_send_message(addressIdTable[address].get(), path, message);
    lo_message_free(message);
  }

};

}
