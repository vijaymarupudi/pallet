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

namespace detail {
struct LoAddressTypeCleanup {
  void operator()(lo_address addr) {
    lo_address_free(addr);
  }
};

using LoAddressType = std::unique_ptr<std::remove_pointer_t<lo_address>, LoAddressTypeCleanup>;

struct LoServerPtrCleanup {
  void operator()(lo_server server) {
    lo_server_free(server);
  }
};

using LoServerPtr = std::unique_ptr<std::remove_pointer_t<lo_server>, LoServerPtrCleanup>;

struct LoServer {
  LoServerPtr loServer;
  OscInterface::MessageEvent event;
  std::vector<OscItem> messageBuffer;
  PosixPlatform* platform;
  int port;

  LoServer(int port, PosixPlatform* iplatform);
  LoServer(LoServer&& other);
  LoServer& operator=(LoServer&& other);
  ~LoServer();

private:
  void uponLoMessage(const char *path, const char *types,
                     lo_arg ** argv,
                     int argc, lo_message data);
  static int uponLoOscMessageCallback(const char *path, const char *types,
                                      lo_arg ** argv,
                                      int argc, lo_message data,
                                      void *user_data);
  static void loServerFdReadyCallback(int fd, short revents, void* userData);
};

}

class LoOscInterface final : public OscInterface {
public:

  static Result<LoOscInterface> create(PosixPlatform& platform);
  LoOscInterface(PosixPlatform& platform);

  virtual AddressId createAddress(int port) override;
  virtual void freeAddress(AddressId id) override;

  virtual ServerId createServer(int port) override;
  virtual void freeServer(ServerId id) override;

  virtual MessageEvent::Id listen(ServerId id, MessageEvent::Callback callback) override;
  virtual void unlisten(ServerId id, MessageEvent::Id eid) override;

private:

  PosixPlatform* platform;
  pallet::containers::IdTable<detail::LoAddressType, std::vector, AddressId> addressIdTable;
  pallet::containers::IdTable<detail::LoServer, std::vector, ServerId> serverIdTable;

  virtual void sendMessageImpl(const AddressId address, const char* path,
                                               const OscItem* items, size_t n) override;

};

}
