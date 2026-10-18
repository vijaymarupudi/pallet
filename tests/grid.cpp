#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "pallet/OscMonomeGridInterface.hpp"
#include <map>
#include <string>
#include <vector>
using namespace pallet;

struct Fake : OscInterface {
  std::map<AddressId,int> addrs; std::map<ServerId,MessageEvent::Callback> servers;
  size_t nextA = 0, live = 0, frees = 0;
  std::vector<std::string> sent; // "port path"
  Result<AddressId> createAddress(int port) override {
    if (port < 1 || port > 65535) return pallet::error(std::make_error_condition(std::errc::invalid_argument));
    addrs[nextA]=port; live++; return nextA++; }
  void freeAddress(AddressId id) override { CHECK(addrs.count(id)); addrs.erase(id); live--; frees++; }
  Result<ServerId> createServer(int) override { return 0; }
  void freeServer(ServerId) override {}
  Result<int> serverPort(ServerId) override { return 54321; }
  Result<MessageEvent::Id> listen(ServerId id, MessageEvent::Callback cb) override { servers.emplace(id, std::move(cb)); return 0; }
  void unlisten(ServerId, MessageEvent::Id) override {}
  std::vector<std::vector<OscItem>> args;
  void sendMessageImpl(AddressId a, const char* p, const OscItem* it, size_t n) override {
    sent.push_back(std::to_string(addrs.at(a)) + " " + p); args.emplace_back(it, it + n);
    if (n && std::string(p)=="/sys/prefix") lastPrefix = std::string(*pallet::get_if<std::string_view>(it[0])); }
  std::string lastPrefix;
  template <class... T> void recv(const char* path, T... args) {
    OscItem items[] = {OscItem(args)..., OscItem(0)};
    servers.at(0)(path, items, sizeof...(T));
  }
  // the arguments of the nth message sent as "port path"
  const std::vector<OscItem>& argsOf(const std::string& key, int nth = 0) {
    for (size_t i = 0; i < sent.size(); i++) if (sent[i] == key && nth-- == 0) return args[i];
    FAIL("message not sent: " << key);
    static const std::vector<OscItem> none;
    return none;
  }
  int count(const std::string& s) { int n=0; for (auto& x: sent) if (x==s) n++; return n; }
};
using namespace std::string_view_literals;

static void announce(Fake& f, const char* id, int port) {
  f.recv("/serialosc/device", std::string_view(id), "monome 128"sv, (int32_t)port);
  f.recv("/sys/id", std::string_view(id));
  f.recv("/sys/size", 16, 8);
  f.recv("/sys/host", "localhost"sv); f.recv("/sys/port", 7072); f.recv("/sys/prefix", "/monome"sv);
  f.recv("/sys/rotation", 0);
}

TEST_CASE("grid: connect, keys, leds, replug") {
  Fake f;
  {
    auto r = OscMonomeGridInterface::create(f);
    REQUIRE(r);
    auto& g = *r;
    REQUIRE((f.count("12002 /serialosc/list") == 1 && f.count("12002 /serialosc/notify") == 1));
    // the grid tells serialosc to use the address and the port of the server
    auto str = [](const OscItem& x) { return std::string(*pallet::get_if<std::string_view>(x)); };
    auto num = [](const OscItem& x) { return *pallet::get_if<int32_t>(x); };
    for (auto key : {"12002 /serialosc/list", "12002 /serialosc/notify"}) {
      auto& a = f.argsOf(key);
      REQUIRE((a.size() == 2 && str(a[0]) == "127.0.0.1" && num(a[1]) == 54321));
    }
    // hostile ports in listings are dropped
    f.recv("/serialosc/device", "bad0"sv, "t"sv, 0);
    f.recv("/serialosc/device", "bad1"sv, "t"sv, 70000);
    f.recv("/serialosc/device", "bad2"sv, "t"sv, -5);
    REQUIRE((g.deviceCount() == 0 && f.sent.size() == 2));

    int connected = -1; int busy = 0;
    // connect before discovery
    g.connect(0, [&](Result<unsigned> x){ if (x) connected = *x; else busy++; });
    REQUIRE(connected == -1);
    announce(f, "m1", 15000);
    REQUIRE((connected == 0 && g.isConnected(0)));
    REQUIRE(f.count("15000 /sys/prefix") == 1);
    {
      auto& host = f.argsOf("15000 /sys/host"); auto& port = f.argsOf("15000 /sys/port");
      auto& info = f.argsOf("15000 /sys/info");
      REQUIRE((str(host[0]) == "127.0.0.1" && num(port[0]) == 54321 && num(info[0]) == 54321));
    }
    // duplicate listing / second connect
    announce(f, "m1", 15000);
    REQUIRE(g.deviceCount() == 1);
    g.connect(0, [&](Result<unsigned> x){ if (!x) busy++; });
    REQUIRE(busy == 1);

    // keys
    int keys = 0; g.listen(0, [&](int,int,int){ keys++; });
    f.recv("/p/0/grid/key", 1, 2, 1); REQUIRE(keys == 1);
    // malformed / hostile input is dropped
    f.recv("/p/0/grid/key", 1, 2); f.recv("/p/0/grid/key", 1, 2, 1.0f);
    f.recv("/p/9/grid/key", 1, 2, 1); f.recv("/p/x/grid/key", 1, 2, 1);
    f.recv("/serialosc/device", 5); f.recv("/serialosc/add", "m1"sv); f.recv("/sys/size", 1); f.recv("/sys/rotation");
    REQUIRE(keys == 1);

    // leds, bounds
    g.led(0, 3, 3, 15); g.led(0, 99, 3, 15); g.led(0, -1, 0, 1); g.led(5, 0, 0, 1);
    g.render(0);
    REQUIRE(f.count("15000 /monome/grid/led/level/map") == 2); // both quads start dirty
    // unplug / replug on a new port
    f.recv("/serialosc/remove", "m1"sv, "monome 128"sv, 15000); REQUIRE(!g.isConnected(0));
    g.render(0);
    size_t before = f.sent.size();
    f.recv("/serialosc/add", "m1"sv, "monome 128"sv, 15001);
    f.recv("/sys/id", "m1"sv); f.recv("/sys/size", 16, 8); f.recv("/sys/size", 16, 8); f.recv("/sys/rotation", 0);
    REQUIRE(f.count("12002 /serialosc/notify") == 3);
    REQUIRE((g.isConnected(0) && g.deviceCount() == 1));
    REQUIRE(f.count("15001 /sys/prefix") == 1);
    REQUIRE(f.count("15001 /monome/grid/led/level/map") == 2);  // full redraw
    (void)before;
    REQUIRE(f.live == 2); // serialoscd + one device; old device address freed
    {
      // prefix/host/port arguments of the replug activation
      bool ok=false;
      for (size_t i=0;i<f.sent.size();i++) if (f.sent[i]=="15001 /sys/prefix") ok = f.lastPrefix=="/p/0";
      REQUIRE(ok);
    }
    // second device keeps its own identity and replies are routed by /sys/id
    announce(f, "m2", 15002);
    REQUIRE(g.deviceCount() == 2);
    REQUIRE(!g.isConnected(1));
    REQUIRE((std::string(g.getId(1)) == "m2" && g.getCols(1) == 16));
  }
  CHECK(f.live == 0);
}

// two devices listed together, replies go to the front one
TEST_CASE("grid: probing is sequential, tall grids, listeners") {
  Fake f2;
  {
    auto r = OscMonomeGridInterface::create(f2); REQUIRE(r); auto& g = *r;
    f2.recv("/serialosc/device", "a"sv, "t"sv, 20001);
    f2.recv("/serialosc/device", "b"sv, "t"sv, 20002);
    REQUIRE((f2.count("20001 /sys/info") == 1 && f2.count("20002 /sys/info") == 0));
    f2.recv("/sys/size", 8, 16); f2.recv("/sys/rotation", 90);
    REQUIRE(f2.count("20002 /sys/info") == 1);
    f2.recv("/sys/size", 16, 16); f2.recv("/sys/rotation", 0);
    REQUIRE((g.getRows(0) == 16 && g.getCols(0) == 8 && g.getRotation(0) == 90 && g.getCols(1) == 16));
    // tall grid: led in the lower-left quad is sent
    int got = -1; g.connect(0, [&](Result<unsigned> x){ got = x ? *x : 99; });
    REQUIRE(got == 0);
    g.clear(0); g.render(0); f2.sent.clear(); f2.args.clear();
    g.led(0, 0, 12, 15); g.render(0);
    REQUIRE(f2.sent.size() == 1);
    REQUIRE(*pallet::get_if<int32_t>(f2.args[0][1]) == 8);   // offY
    // remove the device being probed: queue moves on
    f2.recv("/serialosc/device", "c"sv, "t"sv, 20003);
    f2.recv("/serialosc/device", "d"sv, "t"sv, 20004);
    REQUIRE((f2.count("20003 /sys/info") == 1 && f2.count("20004 /sys/info") == 0));
    f2.recv("/serialosc/remove", "c"sv, "t"sv, 20003);
    REQUIRE(f2.count("20004 /sys/info") == 1);
    // unlisten inside callbacks
    int calls = 0; unsigned a = 0, b = 0;
    a = g.listen(0, [&](int,int,int){ calls++; g.unlisten(0, b); g.unlisten(0, a); g.unlisten(0, 999); });
    b = g.listen(0, [&](int,int,int){ calls += 100; });
    f2.recv("/p/0/grid/key", 0, 0, 1);
    REQUIRE(calls == 1);
    f2.recv("/p/0/grid/key", 0, 0, 1);
    REQUIRE(calls == 1);
    g.unlisten(0, a);
    // listen from inside a callback, enough times to grow the storage
    int outer = 0, inner = 0;
    unsigned c = g.listen(0, [&](int,int,int){ outer++; for (int i = 0; i < 20; i++) g.listen(0, [&](int,int,int){ inner++; }); outer += 0; });
    f2.recv("/p/0/grid/key", 0, 0, 1);
    REQUIRE(outer == 1);
    g.unlisten(0, c);
    f2.recv("/p/0/grid/key", 0, 0, 1);
    REQUIRE(inner >= 20);
  }
  CHECK(f2.live == 0);
}

TEST_CASE("grid: connect by serial") {
  Fake f3;
  {
    auto r = OscMonomeGridInterface::create(f3); REQUIRE(r);
    auto& g = *r;
    int got = -1, busy = 0, other = -1;
    g.connect("m2"sv, [&](Result<unsigned> x){ if (x) got = *x; else busy++; });
    g.connect("m2"sv, [&](Result<unsigned> x){ if (!x) busy++; });
    g.connect("zz"sv, [&](Result<unsigned> x){ if (x) other = *x; });
    REQUIRE((got == -1 && busy == 1));
    announce(f3, "m1", 15000);   // index 0, not m2
    REQUIRE((got == -1 && !g.isConnected(0)));
    announce(f3, "m2", 15001);   // index 1
    REQUIRE((got == 1 && g.isConnected(1) && !g.isConnected(0)));
    REQUIRE(other == -1);
    // already discovered: resolves immediately, busy when claimed
    int again = -1, again_busy = 0;
    g.connect("m1"sv, [&](Result<unsigned> x){ if (x) again = *x; });
    REQUIRE((again == 0 && g.isConnected(0)));
    g.connect("m1"sv, [&](Result<unsigned> x){ if (!x) again_busy++; });
    REQUIRE(again_busy == 1);
    // the request for a serial that has not appeared is still waiting
    announce(f3, "zz", 15002);   // index 2
    REQUIRE((other == 2 && g.isConnected(2)));
  }
  CHECK(f3.live == 0);
}
