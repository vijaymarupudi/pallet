#include "MonomeGridInterface.hpp"

namespace pallet {

static inline int calcQuadIndex(int x, int y) {
  return (x / 8) + (y / 8) * 2;
}

static inline std::pair<int, int> calcQuadIndexAndPointIndex(int x, int y) {
  return std::pair(calcQuadIndex(x, y), (x % 8) + 8 * (y % 8));
}

static Result<LinuxMonomeGridInterface> create(LinuxPlatform& platform) {
  return Result<LinuxMonomeGridInterface>(std::in_place_t{}, platform);
}

MonomeGrid::MonomeGrid(std::string id, int rows, int cols, MonomeGrid::QuadRenderFunc quadRenderFunc,
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


#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

static const int gridOscServerPort = 7072;

  
LinuxMonomeGridInterface::LinuxMonomeGridInterface(LinuxPlatform& platform) : oscInterface(platform) {
  this->gridAddr = nullptr;

  serialoscdAddr = oscInterface.newAddress(12002);
  oscInterface.setOnMessage([](const char *path, const char *types,
                               lo_arg ** argv,
                               int argc, lo_message data, void* ud){
    ((LinuxMonomeGridInterface*)ud)->uponOscMessage(path, types, argv, argc, data);
  }, this);
  this->oscInterface.bind(gridOscServerPort);
  requestDeviceNotifications();
}

void LinuxMonomeGridInterface::sendRawQuadMap(int offX, int offY, MonomeGrid::QuadType data) {
  if (!this->gridAddr) {return;}
  lo_message msg = lo_message_new();
  lo_message_add_int32(msg, offX);
  lo_message_add_int32(msg, offY);
  for (int i = 0; i < 64; i++) {
    lo_message_add_int32(msg, data[i]);
  }
  this->oscInterface.sendMessage(this->gridAddr, "/monome/grid/led/level/map", msg);
  lo_message_free(msg);
}

void LinuxMonomeGridInterface::connect(int id) {
  (void)id;
  this->oscInterface.sendMessage(this->serialoscdAddr, "/serialosc/list", "si",
                                 "localhost", this->oscInterface.port, LO_ARGS_END);
}

void LinuxMonomeGridInterface::uponOscMessage(const char *path, const char *types,
                    lo_arg ** argv,
                    int argc, lo_message data) {
  (void)types;
  (void)argc;
  (void)data;

  if (strcmp(path, "/monome/grid/key") == 0) {
    int x = argv[0]->i;
    int y = argv[1]->i;
    int z = argv[2]->i;
    if (this->grid) {
      this->grid->uponKey(x, y, z);
    }
        
  } else if (strcmp(path, "/serialosc/device") == 0) {
    // currently only support a single grid
    if (this->grid) { return; }
     
    int gridPort = argv[2]->i;
    gridId = &argv[0]->s;
    // printf("%s\n", &argv[1]->s); => "monome zero"
      
    this->gridAddr = this->oscInterface.newAddress(gridPort);

    // set port, set prefix, ask for information
    this->oscInterface.sendMessage(this->gridAddr, "/sys/port", "i", this->oscInterface.port, LO_ARGS_END);
    this->oscInterface.sendMessage(this->gridAddr, "/sys/prefix", "s", "/monome", LO_ARGS_END);
    this->oscInterface.sendMessage(this->gridAddr, "/sys/info", NULL, LO_ARGS_END);
      
  } else if (strcmp(path, "/sys/size") == 0) {
    int rows = argv[0]->i;
    int cols = argv[1]->i;

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
    this->uponDeviceChange(&argv[0]->s, false);
  } else if (strcmp(path, "/serialosc/add") == 0) {
    this->uponDeviceChange(&argv[0]->s, true);
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
  this->oscInterface.sendMessage(this->serialoscdAddr, "/serialosc/notify",
                                 "si", "localhost", this->oscInterface.port, LO_ARGS_END);

}

void LinuxMonomeGridInterface::disconnect(bool manual) {
  if (grid && grid->isConnected()) {
    this->oscInterface.freeAddress(this->gridAddr);
    this->gridAddr = nullptr;
    this->grid->uponConnectionState(false);
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




