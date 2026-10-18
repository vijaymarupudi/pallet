#include "pallet/PosixOscInterface.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pallet/PosixPlatform.hpp"

#include <oscpp/client.hpp>
#include <oscpp/server.hpp>

namespace pallet {

static constexpr int MAX_RECEIVES_PER_WAKEUP = 64;
static constexpr int MAX_BUNDLE_DEPTH = 8;
static constexpr size_t MAX_SEND_SIZE = 8192;

namespace detail {

UniqueFd::UniqueFd(UniqueFd&& other) noexcept : fd(std::exchange(other.fd, -1)) {}

UniqueFd& UniqueFd::operator=(UniqueFd&& other) noexcept {
  if (this != &other) {
    reset();
    fd = std::exchange(other.fd, -1);
  }
  return *this;
}

UniqueFd::~UniqueFd() { reset(); }

void UniqueFd::reset() {
  if (fd >= 0) { close(fd); }
  fd = -1;
}

}

static std::error_condition invalidArgument() {
  return std::make_error_condition(std::errc::invalid_argument);
}

static std::error_condition errnoCondition(int err) {
  return std::error_condition(err, std::generic_category());
}

static bool isValidPort(int port, int min) {
  return port >= min && port <= 65535;
}

static sockaddr_in loopbackAddress(int port) {
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  return addr;
}

static Result<detail::UniqueFd> makeUdpSocket() {
  int fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
  if (fd < 0) { return pallet::error(errnoCondition(errno)); }
  return detail::UniqueFd(fd);
}

// Binds the socket to the loopback interface, so that only this
// machine can reach it, and returns the port. A port of 0 lets the
// system choose.
static Result<int> bindLoopback(const detail::UniqueFd& fd, int port) {
  auto addr = loopbackAddress(port);
  socklen_t len = sizeof(addr);
  if (bind(fd.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0 ||
      getsockname(fd.get(), reinterpret_cast<sockaddr*>(&addr), &len) < 0) {
    return pallet::error(errnoCondition(errno));
  }
  return ntohs(addr.sin_port);
}

/*
 * Receiving
 */

// An invalid message is dropped. A message that cannot be framed also
// drops the rest of its bundle.
static void emitMessage(detail::PosixOscServer& server, const OSCPP::Server::Message& message) {
  auto args = message.args();
  auto& items = server.items;
  items.clear();
  while (!args.atEnd()) {
    auto tag = args.tag();
    if (!tag) { return; }
    switch (*tag) {
    case 'i': {
      auto value = args.int32();
      if (!value) { return; }
      items.emplace_back(*value);
      break;
    }
    case 'f': {
      auto value = args.float32();
      if (!value) { return; }
      items.emplace_back(*value);
      break;
    }
    case 's': {
      auto value = args.string();
      if (!value) { return; }
      items.emplace_back(std::string_view(*value));
      break;
    }
    default:
      // other types have no OscItem, so they are skipped
      if (!args.drop()) { return; }
      break;
    }
  }
  server.event.emit(message.address(), items.data(), items.size());
  items.clear();
}

static void dispatchPacket(detail::PosixOscServer& server, const OSCPP::Server::Packet& packet, int depth) {
  if (packet.isMessage()) {
    if (auto message = packet.message()) { emitMessage(server, *message); }
  } else if (depth < MAX_BUNDLE_DEPTH) {
    auto bundle = packet.bundle();
    if (!bundle) { return; }
    auto packets = bundle->packets();
    while (!packets.atEnd()) {
      auto next = packets.next();
      if (!next) { return; }
      dispatchPacket(server, *next, depth + 1);
      // a listener freed this server
      if (!server.fd) { return; }
    }
  }
}

namespace detail {

PosixOscServer::~PosixOscServer() {
  shutdown();
}

Result<void> PosixOscServer::open(int requestedPort) {
  auto socket = makeUdpSocket();
  if (!socket) { return pallet::error(socket.error()); }
  auto bound = bindLoopback(*socket, requestedPort);
  if (!bound) { return pallet::error(bound.error()); }
  fd = std::move(*socket);
  // the system chose the port if requestedPort was 0
  port = *bound;
  platform->watchFdIn(fd.get(), fdReadyCallback, this);
  return {};
}

void PosixOscServer::shutdown() {
  if (fd) {
    platform->unwatchFdIn(fd.get());
    fd = UniqueFd();
  }
}

void PosixOscServer::fdReadyCallback(int, short, void* userData) {
  static_cast<PosixOscServer*>(userData)->receive();
}

void PosixOscServer::receive() {
  dispatching = true;
  for (int i = 0; i < MAX_RECEIVES_PER_WAKEUP && fd; i++) {
    ssize_t n = recvfrom(fd.get(), buffer, sizeof(buffer), 0, nullptr, nullptr);
    // EAGAIN, or an error that poll reports again
    if (n < 0) { break; }
    dispatchPacket(*this, OSCPP::Server::Packet(buffer, static_cast<size_t>(n)), 0);
  }
  dispatching = false;
  // a listener freed this server while it was running
  if (deleteWhenDone) { delete this; }
}

}

/*
 * Sending
 */

static OSCPP::Result<void> writeMessage(OSCPP::Client::Packet& packet, const char* path,
                                        const OscItem* items, size_t n) {
  auto visitor = overloaded {
    [&](const std::string_view& view) {
      // the view is not necessarily NUL-terminated
      return packet.string(std::string(view).c_str());
    },
    [&](const int32_t& number) {
      return packet.int32(number);
    },
    [&](const float& number) {
      return packet.float32(number);
    }
  };

  auto result = packet.openMessage(path, n);
  for (size_t i = 0; result && i < n; i++) {
    result = pallet::visit(visitor, items[i]);
  }
  return result.and_then([&] { return packet.closeMessage(); });
}

/*
 * PosixOscInterface
 */

Result<PosixOscInterface> PosixOscInterface::create(PosixPlatform& platform) {
  auto sendSocket = makeUdpSocket();
  if (!sendSocket) { return pallet::error(sendSocket.error()); }
  // Nothing reads this socket, but an unbound one would be bound to
  // all interfaces by the first send, and collect what remote hosts
  // send to it.
  auto bound = bindLoopback(*sendSocket, 0);
  if (!bound) { return pallet::error(bound.error()); }
  return Result<PosixOscInterface>(std::in_place, platform, std::move(*sendSocket));
}

PosixOscInterface::PosixOscInterface(PosixPlatform& platform, detail::UniqueFd sendSocket)
  : platform(&platform), sendSocket(std::move(sendSocket)) {}

Result<PosixOscInterface::AddressId> PosixOscInterface::createAddress(int port) {
  if (!isValidPort(port, 1)) { return pallet::error(invalidArgument()); }
  return addressIdTable.push(detail::PosixOscAddress{static_cast<uint16_t>(port)});
}

void PosixOscInterface::freeAddress(AddressId id) {
  if (addressIdTable.contains(id)) { addressIdTable.free(id); }
}

Result<PosixOscInterface::ServerId> PosixOscInterface::createServer(int port) {
  if (!isValidPort(port, 0)) { return pallet::error(invalidArgument()); }
  auto server = std::make_unique<detail::PosixOscServer>(platform);
  if (auto opened = server->open(port); !opened) { return pallet::error(opened.error()); }
  return serverIdTable.push(std::move(server));
}

Result<int> PosixOscInterface::serverPort(ServerId id) {
  if (!serverIdTable.contains(id)) { return pallet::error(invalidArgument()); }
  return serverIdTable[id]->port;
}

void PosixOscInterface::freeServer(ServerId id) {
  if (!serverIdTable.contains(id)) { return; }
  auto& server = serverIdTable[id];
  if (server->dispatching) {
    // A listener is freeing its own server (or one of its siblings in
    // the same dispatch), so the code below it still uses the object.
    // Stop receiving now and let receive() delete it.
    server->shutdown();
    server->deleteWhenDone = true;
    (void)server.release();
  }
  serverIdTable.free(id);
}

Result<PosixOscInterface::MessageEvent::Id> PosixOscInterface::listen(ServerId id, MessageEvent::Callback callback) {
  if (!serverIdTable.contains(id)) { return pallet::error(invalidArgument()); }
  return serverIdTable[id]->event.listen(std::move(callback));
}

void PosixOscInterface::unlisten(ServerId id, MessageEvent::Id eid) {
  if (serverIdTable.contains(id)) { serverIdTable[id]->event.unlisten(eid); }
}

void PosixOscInterface::sendMessageImpl(const AddressId address, const char* path,
                                        const OscItem* items, size_t n) {
  if (!addressIdTable.contains(address)) { return; }
  OSCPP::Client::StaticPacket<MAX_SEND_SIZE> packet;
  if (auto written = writeMessage(packet, path, items, n); !written) {
    fprintf(stderr, "osc: dropping message to %s: %s\n", path, written.error().message().c_str());
    return;
  }
  const sockaddr_in addr = loopbackAddress(addressIdTable[address].port);
  // UDP is lossy, so a failed send is handled like a lost packet
  (void)sendto(sendSocket.get(), packet.data(), packet.size(), 0,
               reinterpret_cast<const sockaddr*>(&addr), sizeof(addr));
}

}
