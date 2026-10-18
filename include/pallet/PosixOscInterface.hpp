#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "pallet/OscInterface.hpp"
#include "pallet/containers/IdTable.hpp"
#include "pallet/error.hpp"

namespace pallet {

class PosixPlatform;

namespace detail {

// Owns a file descriptor and closes it.
class UniqueFd {
public:
  UniqueFd() : fd(-1) {}
  explicit UniqueFd(int fd) : fd(fd) {}
  UniqueFd(UniqueFd&& other) noexcept;
  UniqueFd& operator=(UniqueFd&& other) noexcept;
  ~UniqueFd();

  explicit operator bool() const { return fd >= 0; }
  int get() const { return fd; }

private:
  int fd;

  void reset();
};

// Addresses are on the loopback interface, so the port is all there is.
struct PosixOscAddress {
  uint16_t port;
};

// A UDP socket bound to 127.0.0.1 that emits the OSC messages it
// receives. It is heap allocated because the platform keeps a pointer
// to it.
struct PosixOscServer {
  UniqueFd fd;
  PosixPlatform* platform;
  int port = 0;
  OscInterface::MessageEvent event;
  std::vector<OscItem> items;
  alignas(4) char buffer[65536];
  // Set while listeners may be running. A server that is freed then is
  // shut down at once, but only deleted when receive() returns.
  bool dispatching = false;
  bool deleteWhenDone = false;

  explicit PosixOscServer(PosixPlatform* platform) : platform(platform) {}
  PosixOscServer(const PosixOscServer&) = delete;
  PosixOscServer& operator=(const PosixOscServer&) = delete;
  ~PosixOscServer();

  // 0 for requestedPort lets the system choose
  Result<void> open(int requestedPort);
  // stop receiving and close the socket
  void shutdown();

private:
  static void fdReadyCallback(int fd, short revents, void* userData);
  void receive();
};

}

// OSC over UDP on the loopback interface, using the vendored oscpp for
// the encoding.
class PosixOscInterface final : public OscInterface {
public:

  static Result<PosixOscInterface> create(PosixPlatform& platform);
  PosixOscInterface(PosixPlatform& platform, detail::UniqueFd sendSocket);

  virtual Result<AddressId> createAddress(int port) override;
  virtual void freeAddress(AddressId id) override;

  virtual Result<ServerId> createServer(int port) override;
  virtual Result<int> serverPort(ServerId id) override;
  virtual void freeServer(ServerId id) override;

  virtual Result<MessageEvent::Id> listen(ServerId id, MessageEvent::Callback callback) override;
  virtual void unlisten(ServerId id, MessageEvent::Id eid) override;

private:

  PosixPlatform* platform;
  detail::UniqueFd sendSocket;
  pallet::containers::IdTable<detail::PosixOscAddress, std::vector, AddressId> addressIdTable;
  pallet::containers::IdTable<std::unique_ptr<detail::PosixOscServer>, std::vector, ServerId> serverIdTable;

  virtual void sendMessageImpl(const AddressId address, const char* path,
                               const OscItem* items, size_t n) override;

};

}
