#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "pallet/PosixOscInterface.hpp"
#include "pallet/PosixPlatform.hpp"

using namespace pallet;
using namespace std::string_view_literals;

namespace {

struct Fixture {
  PosixPlatform platform = unwrap(PosixPlatform::create());
  PosixOscInterface iface = unwrap(PosixOscInterface::create(platform));
};

struct UdpSocket {
  unsigned ip;  // as /proc/net/udp prints it
  int port;
};

unsigned loopbackIp() {
  return htonl(INADDR_LOOPBACK);
}

// The IPv4 UDP sockets of this process, from /proc/net/udp.
std::vector<UdpSocket> ownUdpSockets() {
  std::set<unsigned long> inodes;
  for (const auto& entry : std::filesystem::directory_iterator("/proc/self/fd")) {
    std::error_code ec;
    auto target = std::filesystem::read_symlink(entry.path(), ec);
    unsigned long inode;
    if (!ec && sscanf(target.c_str(), "socket:[%lu]", &inode) == 1) { inodes.insert(inode); }
  }
  std::vector<UdpSocket> result;
  std::ifstream udp("/proc/net/udp");
  std::string line;
  std::getline(udp, line);  // header
  while (std::getline(udp, line)) {
    unsigned ip, port;
    unsigned long inode;
    if (sscanf(line.c_str(), " %*d: %x:%x %*x:%*x %*x %*x:%*x %*x:%*x %*x %*u %*u %lu",
               &ip, &port, &inode) == 3 &&
        inodes.count(inode)) {
      result.push_back({ip, static_cast<int>(port)});
    }
  }
  return result;
}

bool listensOn(int port) {
  for (const auto& socket : ownUdpSockets()) {
    if (socket.port == port) { return true; }
  }
  return false;
}

std::string describe(const char* path, const OscItem* items, size_t n) {
  std::string s = path;
  for (size_t i = 0; i < n; i++) {
    if (auto v = pallet::get_if<int32_t>(items[i])) s += " i" + std::to_string(*v);
    else if (auto v = pallet::get_if<float>(items[i])) s += " f" + std::to_string(static_cast<int>(*v));
    else if (auto v = pallet::get_if<std::string_view>(items[i])) s += " s" + std::string(*v);
  }
  return s;
}

}

TEST_CASE("osc: messages travel over the loopback interface") {
  Fixture f;
  auto server = *f.iface.createServer(0);
  int port = *f.iface.serverPort(server);
  REQUIRE(port > 0);
  REQUIRE(listensOn(port));

  std::vector<std::string> got;
  (void)f.iface.listen(server, [&](const char* path, const OscItem* items, size_t n) {
    got.push_back(describe(path, items, n));
    if (got.size() == 3) { f.platform.quit(); }
  });

  // our own sender, including a string view that is not NUL-terminated
  auto addr = *f.iface.createAddress(port);
  std::string_view whole = "abcdef";
  f.iface.send(addr, "/own", 5, 2.0f, whole.substr(0, 3));
  // a 66 value message like the grid sends
  f.iface.send(addr, "/monome/grid/led/level/map", 0, 0,
               0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
               0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
               0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
               0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

  // an external sender: garbage, then a bundle
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in to{};
  to.sin_family = AF_INET;
  to.sin_port = htons(port);
  to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  const char garbage[] = "\xff\xff\xff\xff\xff\xff\xff\xff";
  sendto(s, garbage, sizeof garbage, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
  alignas(4) const char bundle[] = {
    '#', 'b', 'u', 'n', 'd', 'l', 'e', 0, 0, 0, 0, 0, 0, 0, 0, 1,
    0, 0, 0, 12, '/', 'x', 0, 0, ',', 'i', 0, 0, 0, 0, 0, 42};
  sendto(s, bundle, sizeof bundle, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
  close(s);

  f.platform.loop();
  REQUIRE(got.size() == 3);
  CHECK(got[0] == "/own i5 f2 sabc");
  CHECK(got[1].rfind("/monome/grid/led/level/map i0 i0 i0 i1 i2", 0) == 0);
  CHECK(got[2] == "/x i42");

  f.iface.freeServer(server);
  CHECK(!listensOn(port));
}

TEST_CASE("osc: every socket is bound to the loopback interface") {
  Fixture f;
  auto server = *f.iface.createServer(0);
  (void)server;
  auto sockets = ownUdpSockets();
  // the server and the send socket
  REQUIRE(sockets.size() == 2);
  for (const auto& socket : sockets) { CHECK(socket.ip == loopbackIp()); }
}

TEST_CASE("osc: invalid ports, taken ports and stale ids") {
  Fixture f;
  for (int bad : {-1, 65536, 70000}) {
    CHECK(!f.iface.createServer(bad));
    CHECK(!f.iface.createAddress(bad));
  }
  CHECK(!f.iface.createAddress(0));

  auto server = *f.iface.createServer(0);
  auto taken = f.iface.createServer(*f.iface.serverPort(server));
  REQUIRE(!taken);
  CHECK(taken.error() == std::errc::address_in_use);

  // ids that are not live are rejected or ignored
  CHECK(!f.iface.serverPort(12345));
  CHECK(!f.iface.listen(12345, [](const char*, const OscItem*, size_t) {}));
  f.iface.freeServer(12345);
  f.iface.freeAddress(12345);
  f.iface.unlisten(12345, 0);
  f.iface.send(12345, "/x", 1);

  auto tmp = *f.iface.createServer(0);
  f.iface.freeServer(tmp);
  f.iface.freeServer(tmp);  // twice
  CHECK(!f.iface.serverPort(tmp));
}

// A listener frees its own server and a sibling whose socket is ready in
// the same poll.
TEST_CASE("osc: a listener frees servers") {
  Fixture f;
  auto self = *f.iface.createServer(0);
  auto other = *f.iface.createServer(0);
  auto stopper = *f.iface.createServer(0);  // the highest fd, so it runs last
  int selfCalls = 0, otherCalls = 0;
  (void)f.iface.listen(self, [&](const char*, const OscItem*, size_t) {
    selfCalls++;
    f.iface.freeServer(self);
    f.iface.freeServer(other);
    // reuses the freed slots while the old object is still being used
    auto fresh = *f.iface.createServer(0);
    f.iface.freeServer(fresh);
  });
  (void)f.iface.listen(other, [&](const char*, const OscItem*, size_t) { otherCalls++; });
  (void)f.iface.listen(stopper, [&](const char*, const OscItem*, size_t) { f.platform.quit(); });

  auto toSelf = *f.iface.createAddress(*f.iface.serverPort(self));
  auto toOther = *f.iface.createAddress(*f.iface.serverPort(other));
  auto toStopper = *f.iface.createAddress(*f.iface.serverPort(stopper));
  f.iface.send(toSelf, "/x", 1);
  f.iface.send(toSelf, "/x", 2);  // the server is gone by now, so this is not delivered
  f.iface.send(toOther, "/y", 3);
  f.iface.send(toStopper, "/z", 4);
  f.platform.loop();

  CHECK(selfCalls == 1);
  CHECK(otherCalls == 0);
  f.iface.freeServer(stopper);
}
