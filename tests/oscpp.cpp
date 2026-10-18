// no exceptions: the library must not need them
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#include "doctest.h"

#include <oscpp/client.hpp>
#include <oscpp/server.hpp>

#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace OSCPP;

static int walkedMessages = 0;

// reads everything in a packet, returning false on the first error
static bool walk(const Server::Packet& packet, int depth = 0);

static bool walkArgs(Server::ArgStream args) {
  while (!args.atEnd()) {
    auto tag = args.tag();
    if (!tag) return false;
    switch (*tag) {
    case 'i': if (!args.int32()) return false; break;
    case 'f': if (!args.float32()) return false; break;
    case 's': if (!args.string()) return false; break;
    case 'b': if (!args.blob()) return false; break;
    case '[': {
      auto array = args.array();
      if (!array || !walkArgs(*array)) return false;
      break;
    }
    default: if (!args.drop()) return false; break;
    }
  }
  return true;
}

static bool walk(const Server::Packet& packet, int depth) {
  if (packet.isMessage()) {
    auto message = packet.message();
    if (!message) return false;
    walkedMessages++;
    return walkArgs(message->args());
  }
  if (depth > 8) return false;
  auto bundle = packet.bundle();
  if (!bundle) return false;
  auto packets = bundle->packets();
  while (!packets.atEnd()) {
    auto next = packets.next();
    if (!next || !walk(*next, depth + 1)) return false;
  }
  return true;
}

static Result<void> buildBundle(Client::Packet& p) {
  return p.openBundle(1)
    .and_then([&] { return p.openMessage("/a", 2); })
    .and_then([&] { return p.int32(7); })
    .and_then([&] { return p.string("hello"); })
    .and_then([&] { return p.closeMessage(); })
    .and_then([&] { return p.openBundle(2); })
    .and_then([&] { return p.openMessage("/b", 4); })
    .and_then([&] { return p.openArray(); })
    .and_then([&] { return p.float32(1.5f); })
    .and_then([&] { return p.int32(3); })
    .and_then([&] { return p.closeArray(); })
    .and_then([&] { return p.closeMessage(); })
    .and_then([&] { return p.closeBundle(); })
    .and_then([&] { return p.closeBundle(); });
}


TEST_CASE("oscpp: round trip of a message") {
  Client::StaticPacket<256> p;
  auto r = p.openMessage("/sys/info", 3)
    .and_then([&] { return p.int32(-5); })
    .and_then([&] { return p.float32(2.5f); })
    .and_then([&] { return p.string("abc"); })
    .and_then([&] { return p.closeMessage(); });
  REQUIRE(r);
  Server::Packet packet(p.data(), p.size());
  auto message = packet.message();
  REQUIRE(message);
  REQUIRE((*message == "/sys/info"));
  auto args = message->args();
  REQUIRE(args.size() == 3);
  REQUIRE(*args.int32() == -5);
  REQUIRE(*args.float32() == 2.5f);
  REQUIRE(std::string(*args.string()) == "abc");
  REQUIRE(args.atEnd());
  // wrong type: Parse, not a crash
  Server::Packet again(p.data(), p.size());
  auto a2 = again.message()->args();
  auto bad = a2.string();
  REQUIRE((!bad && bad.error() == Error::Parse));
  // the error is a std::error_condition of the oscpp category
  static_assert(std::is_same_v<decltype(bad), Result<const char*>>);
  static_assert(std::is_same_v<decltype(bad)::error_type, std::error_condition>);
  REQUIRE(std::string(bad.error().category().name()) == "oscpp");
  REQUIRE(!bad.error().message().empty());
  // a bundle packet is not a message
  REQUIRE(!again.bundle());
}


TEST_CASE("oscpp: nested bundles round trip and truncation") {
  Client::StaticPacket<512> p;
  REQUIRE(buildBundle(p));
  int before = walkedMessages;
  REQUIRE(walk(Server::Packet(p.data(), p.size())));
  REQUIRE(walkedMessages == before + 2);

  // every truncation fails cleanly or parses, but never crashes
  for (size_t n = 0; n <= p.size(); n++) {
    alignas(4) char buf[512];
    memcpy(buf, p.data(), n);
    (void)walk(Server::Packet(buf, n));
  }
  // the whole packet minus 4 bytes is an error
  alignas(4) char buf[512];
  memcpy(buf, p.data(), p.size() - 4);
  REQUIRE(!walk(Server::Packet(buf, p.size() - 4)));
}


TEST_CASE("oscpp: writer errors") {
  Client::StaticPacket<8> tiny;
  auto r = tiny.openMessage("/too/long/address", 0);
  REQUIRE((!r && r.error() == Error::Overflow));

  Client::StaticPacket<64> p;
  auto c = p.closeBundle();
  REQUIRE((!c && c.error() == Error::InvalidArgument));
  REQUIRE(p.openMessage("/x", 0));
  auto nested = p.openBundle(0);
  REQUIRE((!nested && nested.error() == Error::InvalidArgument));

  // more arguments than declared
  p.reset();
  REQUIRE(p.openMessage("/x", 1));
  REQUIRE(p.int32(1));
  auto extra = p.int32(2);
  REQUIRE((!extra && extra.error() == Error::Overflow));

  // unaligned buffer
  alignas(4) char raw[64];
  Client::Packet un(raw + 1, 60);
  REQUIRE(un.openMessage("/x", 1));
  auto u = un.int32(1);
  REQUIRE((!u && u.error() == Error::Unaligned));
}


TEST_CASE("oscpp: dropping a type consumes exactly its data") {
  struct Case { const char* tags; std::vector<unsigned char> data; };
  std::vector<Case> cases = {
    {",di", {0x3f,0xf0,0,0,0,0,0,0}}, {",hi", {0,0,0,0,0,0,0,9}}, {",ti", {0,0,0,0,0,0,0,9}},
    {",ci", {0,0,0,65}}, {",ri", {1,2,3,4}}, {",mi", {1,2,3,4}},
    {",Si", {'a','b','c',0}}, {",Ti", {}}, {",Fi", {}}, {",Ni", {}}, {",Ii", {}}, {",[ii]i", {0,0,0,1,0,0,0,2}}};
  for (auto& c : cases) {
    alignas(4) unsigned char b[64] = {'/','x',0,0};
    size_t n = 4;
    size_t tl = strlen(c.tags) + 1; tl = (tl + 3) & ~size_t(3);
    memcpy(b + n, c.tags, strlen(c.tags)); n += tl;
    if (!c.data.empty()) memcpy(b + n, c.data.data(), c.data.size());
    n += c.data.size();
    b[n+3] = 5; n += 4;   // trailing int32 5
    auto args = Server::Packet(b, n).message()->args();
    while (args.size() > 1 && *args.tag() != 'i') REQUIRE(args.drop());
    if (*args.tag() == '[') REQUIRE(args.drop());
    auto v = args.int32();
    INFO("tags " << c.tags);
    REQUIRE(v);
    REQUIRE(*v == 5);
  }
  // an unknown tag cannot be skipped
  alignas(4) unsigned char u[] = {'/','x',0,0, ',','q','i',0, 0,0,0,5};
  auto args = Server::Packet(u, sizeof u).message()->args();
  auto dropped = args.drop();
  REQUIRE((!dropped && dropped.error() == Error::Parse));
}


TEST_CASE("oscpp: hostile input") {
  // a bundle whose element size is negative
  alignas(4) char evil[32] = {'#','b','u','n','d','l','e',0, 0,0,0,0,0,0,0,1, (char)0xff,(char)0xff,(char)0xff,(char)0xf0};
  REQUIRE(!walk(Server::Packet(evil, 20)));
  // a tag string without a comma
  alignas(4) char notag[16] = {'/','a',0,0, 'i','i',0,0, 0,0,0,1};
  REQUIRE(!walk(Server::Packet(notag, 12)));

  Client::StaticPacket<512> p;
  REQUIRE(buildBundle(p));
  std::mt19937 rng(1234);
  long okCount = 0;
  for (int iter = 0; iter < 300000; iter++) {
    alignas(4) char buf[512];
    size_t n = p.size();
    memcpy(buf, p.data(), n);
    int flips = 1 + rng() % 4;
    for (int i = 0; i < flips; i++) buf[rng() % n] = (char)rng();
    if (rng() % 4 == 0) n = rng() % (n + 1);
    if (walk(Server::Packet(buf, n))) okCount++;
  }
  MESSAGE("fuzz: " << okCount << " of 300000 mutated packets still parse");
  // pure noise
  for (int iter = 0; iter < 100000; iter++) {
    alignas(4) char buf[64];
    size_t n = rng() % 64;
    for (size_t i = 0; i < n; i++) buf[i] = (char)rng();
    if (n > 8 && rng() % 2) memcpy(buf, "#bundle", 8);
    walk(Server::Packet(buf, n));
  }
}

TEST_CASE("oscpp: float to int conversion is checked") {
  auto parse = [](uint32_t bits) {
    alignas(4) unsigned char b[] = {'/', 'x', 0, 0, ',', 'f', 0, 0,
                                    (unsigned char)(bits >> 24), (unsigned char)(bits >> 16),
                                    (unsigned char)(bits >> 8), (unsigned char)bits};
    return Server::Packet(b, sizeof b).message()->args().int32();
  };
  auto seven = parse(0x40fccccd);  // 7.9
  REQUIRE(seven);
  CHECK(*seven == 7);
  CHECK(!parse(0x7fc00000));       // NaN
  CHECK(!parse(0x4f32d05e));       // about 3e9, above INT32_MAX
  CHECK(!parse(0xcf32d05e));       // about -3e9
  CHECK(!parse(0x7f800000));       // infinity
  auto min = parse(0xcf000000);    // -2^31 exactly
  REQUIRE(min);
  CHECK(*min == INT32_MIN);
}

TEST_CASE("oscpp: closeMessage needs openMessage") {
  Client::StaticPacket<128> p;
  REQUIRE(p.openBundle(0));
  auto close = p.closeMessage();
  REQUIRE(!close);
  CHECK(close.error() == Error::InvalidArgument);

  p.reset();
  REQUIRE(p.openBundle(0));
  REQUIRE(p.openMessage("/x", 0));
  REQUIRE(p.closeMessage());
  close = p.closeMessage();  // twice
  REQUIRE(!close);
  CHECK(close.error() == Error::InvalidArgument);
}

TEST_CASE("oscpp: strings at the end of the buffer") {
  // the terminator is the last byte
  alignas(4) unsigned char ok[] = {'/', 'x', 0, 0, ',', 's', 0, 0, 'a', 'b', 'c', 0};
  auto s = Server::Packet(ok, sizeof ok).message()->args().string();
  REQUIRE(s);
  CHECK(std::string(*s) == "abc");
  // no terminator before the end
  alignas(4) unsigned char bad[] = {'/', 'x', 0, 0, ',', 's', 0, 0, 'a', 'b', 'c', 'd'};
  auto b = Server::Packet(bad, sizeof bad).message()->args().string();
  REQUIRE(!b);
  CHECK(b.error() == Error::Underrun);
  // the terminator is in the middle of a group, so the string is not padded
  alignas(4) unsigned char unpadded[] = {'/', 'x', 0, 0, ',', 's', 0, 0, 'a', 0, 'c', 'd'};
  CHECK(!Server::Packet(unpadded, sizeof unpadded).message()->args().string());
}

TEST_CASE("oscpp: packets are not copyable") {
  static_assert(!std::is_copy_constructible_v<Client::Packet>);
  static_assert(!std::is_copy_assignable_v<Client::Packet>);
  static_assert(!std::is_copy_constructible_v<Client::StaticPacket<64>>);
}
