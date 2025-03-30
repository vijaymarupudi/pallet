#include <cstdint>
#include <string_view>
#include <tuple>

#include "variant.hpp"

#include "MonomeGridInterface.hpp"


namespace pallet {

static inline int calcQuadIndex(int x, int y) {
  return (x / 8) + (y / 8) * 2;
}

static inline std::pair<int, int> calcQuadIndexAndPointIndex(int x, int y) {
  return std::pair(calcQuadIndex(x, y), (x % 8) + 8 * (y % 8));
}


MonomeGrid::MonomeGrid(std::string id, int rows, int cols, MonomeGrid::QuadRenderFunction quadRenderFunc,
           void* quadRenderFuncUd0, void* quadRenderFuncUd1) :
  id(std::move(id)), rows(rows), cols(cols), nQuads((rows / 8) * (cols / 8)), quadRenderFunc(quadRenderFunc),
  quadRenderFuncUd0(quadRenderFuncUd0), quadRenderFuncUd1(quadRenderFuncUd1)
{
  memset(&this->quadData[0][0], 0, sizeof(MonomeGrid::QuadType) * 4);
}

void MonomeGrid::setQuadDirty(int quadIndex) {
  this->quadDirtyFlags |= (1 << quadIndex);
}

bool MonomeGrid::isQuadDirty(int quadIndex) {
  return this->quadDirtyFlags & (1 << quadIndex);
}

void MonomeGrid::setOnKey(void (*cb)(int x, int y, int z, void*), void* data) {
  this->onKeyCb = cb;
  this->onKeyData = data;
}

void MonomeGrid::uponKey(int x, int y, int z) {
  if (this->onKeyCb) {
    this->onKeyCb(x, y, z, onKeyData);
  }
}

void MonomeGrid::uponConnectionState(bool state) {
  this->connected = state;
}

bool MonomeGrid::isConnected() {
  return this->connected;
}

void MonomeGrid::led(int x, int y, int c) {
  const auto [quadIndex, pointIndex] = calcQuadIndexAndPointIndex(x, y);
  quadData[quadIndex][pointIndex] = c;
  this->setQuadDirty(quadIndex);
}

void MonomeGrid::all(int z) {
  memset(&quadData, z, sizeof(MonomeGrid::QuadType) * 4);
  for (int i = 0; i < 4; i++) {
    this->setQuadDirty(i);
  }
}

void MonomeGrid::clear() {
  memset(&quadData, 0, sizeof(MonomeGrid::QuadType) * 4);
  for (int i = 0; i < 4; i++) {
    this->setQuadDirty(i);
  }
}

void MonomeGrid::render() {
  if (!this->connected) { return; }
  for (int quadIndex = 0; quadIndex < this->nQuads; quadIndex++) {
    if (this->isQuadDirty(quadIndex)) {
      int offX = (quadIndex % 2) * 8;
      int offY = (quadIndex / 2) * 8;
      this->quadRenderFunc(offX, offY, quadData[quadIndex], this->quadRenderFuncUd0, this->quadRenderFuncUd1);
    }
  }
  this->quadDirtyFlags = 0;
}

int MonomeGrid::getRows() {
  return this->rows;
}

int MonomeGrid::getCols() {
  return this->cols;
}


  void MonomeGridInterface::uponConnectionState(MonomeGrid& g, bool b) {
    g.uponConnectionState(b);
  }
  void MonomeGridInterface::uponKey(MonomeGrid& g, int x, int y, int z) {
    g.uponKey(x, y, z);
  }


#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

static const int gridOscServerPort = 7072;


  template <class... Types>
  std::tuple<Types...> extractOscValues(const OscItem* items) {
    auto tmp = ([&]<class... T, size_t... index>(std::index_sequence<index...>) {
        return std::make_tuple(get_unchecked<T>(items[index])...);
    });

    return tmp.template operator()<Types...>(std::make_index_sequence<sizeof...(Types)>{});
  }

Result<LinuxMonomeGridInterface> LinuxMonomeGridInterface::create(PosixPlatform& platform) {
  return Result<LinuxMonomeGridInterface>(std::in_place_t{}, platform);
}

LinuxMonomeGridInterface::LinuxMonomeGridInterface(PosixPlatform& platform) : oscInterface(platform) {
  serialoscdAddr = oscInterface.createAddress(12002);
  oscInterface.setOnMessage([](const char *path, const OscItem* items,
                               size_t n, void* ud){
    ((LinuxMonomeGridInterface*)ud)->uponOscMessage(path, items, n);
  }, this);
  this->oscInterface.bind(gridOscServerPort);
  requestDeviceNotifications();
}

void LinuxMonomeGridInterface::sendRawQuadMap(int offX, int offY, MonomeGrid::QuadType data) {

  auto tmp = [&]<size_t... indexes>(std::index_sequence<indexes...>){
    this->oscInterface.send(this->gridAddr,
                            "/monome/grid/led/level/map", offX, offY, (static_cast<int32_t>(data[indexes]))...);
  };

  tmp(std::make_index_sequence<64>{});
  // this->oscInterface.sendMessage(this->gridAddr, "/monome/grid/led/level/map", msg);
}

void LinuxMonomeGridInterface::connect(int id) {
  (void)id;
  this->oscInterface.send(this->serialoscdAddr, "/serialosc/list",
                          "localhost", this->oscInterface.port);
}

void LinuxMonomeGridInterface::uponOscMessage(const char *path, const OscItem* items,
                                              size_t n) {

  (void)n;

  if (strcmp(path, "/monome/grid/key") == 0) {

    auto [x, y, z] = extractOscValues<int32_t, int32_t, int32_t>(items);

    if (this->grid) {
      MonomeGridInterface::uponKey(*this->grid, x, y, z);
    }

  } else if (strcmp(path, "/serialosc/device") == 0) {
    // currently only support a single grid
    if (this->grid) { return; }

    int gridPort = get_unchecked<int32_t>(items[2]);
    this->gridId = get_unchecked<std::string_view>(items[0]);

    // printf("%s\n", &argv[1]->s); => "monome zero"

    this->gridAddr = this->oscInterface.createAddress(gridPort);

    // set port, set prefix, ask for information

    this->oscInterface.send(this->gridAddr, "/sys/port", this->oscInterface.port);
    this->oscInterface.send(this->gridAddr, "/sys/prefix", "/monome");
    this->oscInterface.send(this->gridAddr, "/sys/info");

  } else if (strcmp(path, "/sys/size") == 0) {
    auto [rows, cols] = extractOscValues<int32_t, int32_t >(items);

    auto renderFunc = [](int offX, int offY, uint8_t* data, void* ud0, void* ud1) {
      (void)ud1;
      auto thi = (LinuxMonomeGridInterface*)ud0;
      thi->sendRawQuadMap(offX, offY, data);
    };


    this->grid = MonomeGrid(gridId, rows, cols, renderFunc, this, nullptr);

    if (this->onConnectCb) {
      this->onConnectCb(this->gridId, true, &(*this->grid), this->onConnectData);
    }

  } else if (strcmp(path, "/serialosc/remove") == 0) {
    const char* id = get_unchecked<std::string_view>(items[0]).data();
    this->uponDeviceChange(id, false);
  } else if (strcmp(path, "/serialosc/add") == 0) {
    const char* id = get_unchecked<std::string_view>(items[0]).data();
    this->uponDeviceChange(id, true);
  }
}

void LinuxMonomeGridInterface::uponDeviceChange(const char* cStrId, bool addition) {
  std::string deviceId = cStrId;
  if (!addition && deviceId == this->gridId) {
    this->disconnect(false);
  }

  if (addition && deviceId == this->gridId && this->autoReconnect) {
    this->connect(0);
  }
  this->requestDeviceNotifications();
}

void LinuxMonomeGridInterface::requestDeviceNotifications() {
  this->oscInterface.send(this->serialoscdAddr, "/serialosc/notify",
                          "localhost", this->oscInterface.port);

}

void LinuxMonomeGridInterface::disconnect(bool manual) {
  if (grid && grid->isConnected()) {
    this->oscInterface.freeAddress(this->gridAddr);
    MonomeGridInterface::uponConnectionState(*this->grid, false);
    if (manual) {
      this->autoReconnect = false;
    }
    if (this->onConnectCb) {
      this->onConnectCb(this->gridId, false, nullptr, this->onConnectData);
    }
  }
}

LinuxMonomeGridInterface::~LinuxMonomeGridInterface() {
  this->disconnect();
  this->oscInterface.freeAddress(this->serialoscdAddr);
}

#endif

}
