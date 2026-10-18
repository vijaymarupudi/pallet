#include "pallet/LoOscInterface.hpp"

namespace pallet {

static lo_message oscMessageToLoMessage(const OscItem* items, size_t n) {
  lo_message ret = lo_message_new();
  auto visitor = overloaded {
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

static LoAddressType makeLoAddress(int port) {
  char buf[16];
  snprintf(buf, 16, "%d", port);
  return LoAddressType {lo_address_new(NULL, buf)};
}

static LoServerPtr makeLoServer(int port) {
  char buf[16];
  snprintf(buf, 16, "%d", port);
  // a null port lets liblo choose a free one
  return LoServerPtr {lo_server_new(port == 0 ? nullptr : buf, oscInterfaceLoOnErrorFunc)};
}

// liblo ignores the iface and ip settings for UDP servers, so the
// socket listens on every interface. Messages are not authenticated,
// so drop everything that does not come from this machine.
static bool isLoopbackSource(lo_message message) {
  lo_address source = lo_message_get_source(message);
  const char* host = source ? lo_address_get_hostname(source) : nullptr;
  return host && strncmp(host, "127.", 4) == 0;
}


LoServer::LoServer(int port, PosixPlatform* iplatform) : loServer(makeLoServer(port)), platform(iplatform), port(port) {
  if (this->loServer) { this->port = lo_server_get_port(this->loServer.get()); }
  lo_server_add_method(this->loServer.get(), NULL, NULL, LoServer::uponLoOscMessageCallback, this);
  int fd = lo_server_get_socket_fd(this->loServer.get());
  this->platform->watchFdIn(fd, LoServer::loServerFdReadyCallback, this);
}

LoServer::LoServer(LoServer&& other) :
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

LoServer& LoServer::operator=(LoServer&& other) {
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

LoServer::~LoServer() {
  if (loServer) {
    int fd = lo_server_get_socket_fd(this->loServer.get());
    this->platform->unwatchFdIn(fd);
  }
}


void LoServer::uponLoMessage(const char *path, const char *types,
                   lo_arg ** argv,
                   int argc, lo_message data) {
  if (!isLoopbackSource(data)) { return; }
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
  
int LoServer::uponLoOscMessageCallback(const char *path, const char *types,
                                    lo_arg ** argv,
                                    int argc, lo_message data,
                                    void *user_data) {
  static_cast<LoServer*>(user_data)->uponLoMessage(path, types, argv, argc, data);
  return 0;
}

void LoServer::loServerFdReadyCallback(int fd, short revents, void* userData) {
  (void)fd;
  (void)revents;
  auto& loServer = static_cast<LoServer*>(userData)->loServer;
  lo_server_recv_noblock(loServer.get(), 0);
}

}



Result<LoOscInterface> LoOscInterface::create(PosixPlatform& platform) {
  return Result<LoOscInterface>(std::in_place_t{}, platform);
}

LoOscInterface::LoOscInterface(PosixPlatform& platform) : platform(&platform) {}

LoOscInterface::AddressId LoOscInterface::createAddress(int port) {
  auto addr = detail::makeLoAddress(port);
  auto id = addressIdTable.push(std::move(addr));
  return id;
}

void LoOscInterface::freeAddress(AddressId id) {
  addressIdTable.free(id);
}

LoOscInterface::ServerId LoOscInterface::createServer(int port) {
  return serverIdTable.emplace(port, platform);
}

int LoOscInterface::serverPort(ServerId id) {
  return serverIdTable[id].port;
}

void LoOscInterface::freeServer(ServerId id) {
  serverIdTable.free(id);
}

LoOscInterface::MessageEvent::Id LoOscInterface::listen(ServerId id, MessageEvent::Callback callback) {
  return serverIdTable[id].event.listen(std::move(callback));
}
  
void LoOscInterface::unlisten(ServerId id, MessageEvent::Id eid) {
  serverIdTable[id].event.unlisten(eid);
}

void LoOscInterface::sendMessageImpl(const AddressId address, const char* path,
                                     const OscItem* items, size_t n) {
  auto message = oscMessageToLoMessage(items, n);
  lo_send_message(addressIdTable[address].get(), path, message);
  lo_message_free(message);
}

}
