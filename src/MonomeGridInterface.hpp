#pragma once

#include "Platform.hpp"
#include <utility>
#include <string.h>
#include <string>
#include <cinttypes>
#include <optional>
#include "constants.hpp"

namespace pallet {

static inline int calcQuadIndex(int x, int y) {
  return (x / 8) + (y / 8) * 2;
}

static inline std::pair<int, int> calcQuadIndexAndPointIndex(int x, int y) {
  return std::pair(calcQuadIndex(x, y), (x % 8) + 8 * (y % 8));
}



using MonomeGridQuadRenderFunc = void(*)(int offX, int offY, uint8_t* data, void* ud0, void* ud1);
using MonomeGridQuadDataType = uint8_t[64];

class MonomeGrid {
public:

  std::string id;
  int rows;
  int cols;
  int nQuads;
  
  bool connected = true;

  MonomeGridQuadRenderFunc quadRenderFunc;
  void* quadRenderFuncUd0;
  void* quadRenderFuncUd1;

  void (*onKeyCb)(int x, int y, int z, void*) = nullptr;
  void* onKeyData = nullptr;

  MonomeGridQuadDataType quadData[4];
  uint8_t quadDirtyFlags = 0xFF;

  MonomeGrid(std::string id, int rows, int cols, MonomeGridQuadRenderFunc quadRenderFunc,
             void* quadRenderFuncUd0, void* quadRenderFuncUd1) :
    id(std::move(id)), rows(rows), cols(cols), nQuads((rows / 8) * (cols / 8)), quadRenderFunc(quadRenderFunc),
    quadRenderFuncUd0(quadRenderFuncUd0), quadRenderFuncUd1(quadRenderFuncUd1)
  {
    memset(&this->quadData[0][0], 0, sizeof(MonomeGridQuadDataType) * 4);
  }

  void setQuadDirty(int quadIndex) {
    this->quadDirtyFlags |= (1 << quadIndex);
  }

  bool isQuadDirty(int quadIndex) {
    return this->quadDirtyFlags & (1 << quadIndex);
  }

public:
  void setOnKey(void (*cb)(int x, int y, int z, void*), void* data) {
    this->onKeyCb = cb;
    this->onKeyData = data;
  }

  void uponKey(int x, int y, int z) {
    if (this->onKeyCb) {
      this->onKeyCb(x, y, z, onKeyData);
    }
  }

  void uponConnectionState(bool state) {
    this->connected = state;
  }

  bool isConnected() {
    return this->connected;
  }

  void led(int x, int y, int c) {
    const auto [quadIndex, pointIndex] = calcQuadIndexAndPointIndex(x, y);
    quadData[quadIndex][pointIndex] = c;
    this->setQuadDirty(quadIndex);
  }

  void clear() {
    memset(&quadData, 0, sizeof(MonomeGridQuadDataType) * 4);
    for (int i = 0; i < 4; i++) {
      this->setQuadDirty(i);
    }
  }

  void render() {
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

  int getRows() {
    return this->rows;
  }

  int getCols() {
    return this->cols;
  }
};


using MonomeGridInterfaceConnectCallback = void(*)(const std::string&, bool, MonomeGrid* grid, void*);

class MonomeGridInterface {
protected:
  MonomeGridInterfaceConnectCallback onConnectCb = nullptr;
  void* onConnectData = nullptr;

public:

  void setOnConnect(MonomeGridInterfaceConnectCallback cb, void* data) {
    this->onConnectCb = cb;
    this->onConnectData = data;
  }

  virtual void sendRawQuadMap(int offX, int offY, MonomeGridQuadDataType data) = 0;
  virtual void connect(int idx) = 0;
};

}

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

#include "OscInterface.hpp"


namespace pallet {

static const int gridOscServerPort = 7072;

class LinuxMonomeGridInterface : public MonomeGridInterface {
public:
  using address_type = LinuxOscInterface::address_type;
  address_type serialoscdAddr;
  address_type gridAddr;
  LinuxOscInterface oscInterface;
  std::string gridId;
  std::optional<MonomeGrid> grid;
  bool autoReconnect = true;
  
  LinuxMonomeGridInterface(LinuxPlatform& platform) : oscInterface(platform) {
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

  void sendRawQuadMap(int offX, int offY, MonomeGridQuadDataType data) override {
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

  void connect(int id) override {
    (void)id;
    this->oscInterface.sendMessage(this->serialoscdAddr, "/serialosc/list", "si",
                                   "localhost", this->oscInterface.port, LO_ARGS_END);
  }

  void uponOscMessage(const char *path, const char *types,
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

  void uponDeviceChange(const char* cStrId, bool addition) {
    std::string deviceId = cStrId;
    if (!addition && deviceId == this->gridId) {
      this->disconnect(false);
    }

    if (addition && deviceId == this->gridId && this->autoReconnect) {
      this->connect(0);
    }
    this->requestDeviceNotifications();
  }

  void requestDeviceNotifications() {
    this->oscInterface.sendMessage(this->serialoscdAddr, "/serialosc/notify",
                                   "si", "localhost", this->oscInterface.port, LO_ARGS_END);

  }

  void disconnect(bool manual = true) {
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

  ~LinuxMonomeGridInterface() {
    this->disconnect();
    this->oscInterface.freeAddress(this->serialoscdAddr);
  }
};

  // Raw bytes

  // #include <termios.h>

  // int fd = open("/dev/ttyACM0", O_NONBLOCK | O_RDWR);

  // if (fd < 0) {
  //   perror("Error: ");
  //   exit(1);
  // }

  // struct termios params;
  // tcgetattr(fd, &params);
  // cfsetospeed(&params, 115200);
  // cfsetispeed(&params, 115200);
  // /* parity (8N1) */
  // params.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
  // params.c_cflag |=  (CS8 | CLOCAL | CREAD);

  // /* no line processing */
  // params.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);

  // /* raw input */
  // params.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK |
  //                 INPCK | ISTRIP | IXON);

  // /* raw output */
  // params.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR |
  //                 OFILL | OPOST);

  // params.c_cc[VMIN]  = 1;
  // params.c_cc[VTIME] = 0;

  // tcsetattr(fd, TCSANOW, &params);

  // platform.watchFdIn(fd, [](int fd, void* ud) {
  //   char buf[1024];
  //   ssize_t len = read(fd, buf, 1024);

  //   if (len < 0) {
  //     perror("Error: ");
  //     exit(1);
  //   }

  //   if (len % 3 != 0) {
  //     fprintf(stderr, "Error in reading grid serial\n");
  //   }

  //   for (int i = 0; i < len; i += 3) {
  //     unsigned char b1 = buf[i];
  //     unsigned char b2 = buf[i + 1];
  //     unsigned char b3 = buf[i + 2];

  //     char buf[1024];


  //     if (b1 == (2 << 4) + 0x00) {
  //       snprintf(buf, 1024, "v%dl0Z", voiceNumber);
  //       amy_play_message(buf);
  //       // voiceNumber = (voiceNumber + 1) % nVoices;
  //     }

  //     if (b1 == (2 << 4) + 0x01) {
  //       snprintf(buf, 1024, "v%dl1n%dZ", voiceNumber, scale(b2 + b3 * 16) % 127);
  //       amy_play_message(buf);
  //     }
  //   }
  // }, nullptr);


}

#endif
