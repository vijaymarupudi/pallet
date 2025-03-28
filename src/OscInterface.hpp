#pragma once

#include <string_view>
#include <cstdint>
#include <vector>

#include "variant.hpp"


namespace pallet {

using OscItem = Variant<std::string_view, int32_t, float>;

class OscMessage {

public:
  std::vector<OscItem> items;

  void add(auto&& arg) {
    items.emplace_back(std::forward<decltype(arg)>(arg));
  }

  void clear() {
    items.clear();
  }
};

class OscInterface {
  using AddressType = size_t;
  using OnMessageCallback = void(*)(const char* path, const OscMessage&, void*);

  std::vector<OscMessage> freeMessages;
  OscInterfaceCallback onMessageCb;
  void* onMessageUserData;

  virtual AddressType createAddress(int port) = 0;
  virtual void freeAddress(AddressType) = 0;
  virtual void sendMessageImpl(const AddressType address, const OscMessage& msg) = 0;

  void freeMessage(OscMessage&& msg) {
    msg.clear();
    freeMessages.push_back(std::move(msg));
  }
  
  void sendMessage(const AddressType address, OscMessage&& msg) {
    sendMessageImpl(address, msg);
    freeMessage(std::move(msg));
  }

  OscMessage createMessage() {
    if (freeMessages.empty()) {
      return OscMessage{};
    } else {
      auto msg = std::move(freeMessages.back());
      freeMessages.pop_back();
      return msg;
    }
  }
};

}

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX
#include <stdarg.h>
#include "lo/lo.h"
#include "Platform.hpp"

namespace pallet {


inline lo_message oscMessageToLoMessage(const OscMessage& message) {
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
  
  for (const auto& item : message.items) {
    std::visit(visitor, item);
  }

  return ret;
}


static void oscInterfaceLoOnErrorFunc(int num, const char *msg, const char *path)
{
  printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

typedef void(*OscInterfaceCallback)(const char *path,
                                    const char *types,
                                    lo_arg ** argv,
                                    int argc, lo_message data,
                                    void *userData);


static int oscInterfaceLoGenericOscCallback(const char *path, const char *types,
                                                 lo_arg ** argv,
                                                 int argc, lo_message data, void *user_data);

static void oscInterfaceLoServerFdReadyCallback(int fd, short revents, void* userData);


class LinuxOscInterface final : public OscInterface {
public:
  using address_type = lo_address;
  LinuxPlatform* platform;
  lo_server server = nullptr;
  int port;
  OscInterfaceCallback onMessageCb;
  void* onMessageUserData;

  LinuxOscInterface(LinuxPlatform& platform) : platform(&platform) {
    onMessageCb = nullptr;
    onMessageUserData = nullptr;
  }

  void setOnMessage(OscInterfaceCallback cb, void* userData) {
    this->onMessageCb = cb;
    this->onMessageUserData = userData;
  }

  void bind(int port) {
    this->port = port;
    char buf[16];
    snprintf(buf, 16, "%d", port);
    // serverAddr = lo_address_new(NULL, buf);
    server = lo_server_new(buf, oscInterfaceLoOnErrorFunc);
    lo_server_add_method(this->server, NULL, NULL, oscInterfaceLoGenericOscCallback, this);
    int fd = lo_server_get_socket_fd(this->server);
    this->platform->watchFdIn(fd, oscInterfaceLoServerFdReadyCallback, this);
  }

  void uponMessage(const char *path, const char *types,
                   lo_arg ** argv,
                   int argc, lo_message data) {
    if (this->onMessageCb) {
      this->onMessageCb(path, types,
                        argv,
                        argc, data,
                        this->onMessageUserData);
    }
  }

  void sendMessage(address_type addr, const char* path, lo_message message) {
    lo_send_message(addr, path, message);
  }

  virtual void sendMessage(const OscMessage& message) override {
    auto message = oscMessageToLoMessage(message);
    lo_message_free(message);
  }

  void sendMessage(address_type addr, const char* path, const char* type, ...) {
    lo_message msg = lo_message_new();
    va_list args;
    va_start(args, type);
    lo_message_add_varargs(msg, type, args);
    this->sendMessage(addr, path, msg);
    lo_message_free(msg);
  }

  address_type newAddress(int port) {
    char buf[16];
    snprintf(buf, 16, "%d", port);
    return lo_address_new(NULL, buf);
  }

  void freeAddress(address_type addr) {
    lo_address_free(addr);
  }

  ~LinuxOscInterface() {
    int fd = lo_server_get_socket_fd(this->server);
    this->platform->removeFd(fd);
    lo_server_free(server);
  }
};


static int oscInterfaceLoGenericOscCallback(const char *path, const char *types,
                                                 lo_arg ** argv,
                                                 int argc, lo_message data, void *user_data)

{
  ((LinuxOscInterface*)user_data)->uponMessage(path, types, argv, argc, data);
  return 0;
}

static void oscInterfaceLoServerFdReadyCallback(int fd, short revents, void* userData) {
  (void)fd;
  (void)revents;
  auto server = ((LinuxOscInterface*)userData)->server;
  lo_server_recv_noblock(server, 0);
}

}

#endif
