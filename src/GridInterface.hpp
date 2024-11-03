#include "Platform.hpp"
#include <utility>
#include <string.h>
#include <string>

static int calcQuadIndex(int x, int y) {
  return (x / 8) + (y / 8) * 2;
}

static std::pair<int, int> calcQuadIndexAndPointIndex(int x, int y) {
  return std::pair(calcQuadIndex(x, y), (x % 8) + 8 * (y % 8));
}

using ConnectCallbackT = void(*)(const std::string&, bool, void*);

class MonomeGridInterface {
protected:
  ConnectCallbackT onConnectCb = nullptr;
  void* onConnectData = nullptr;

  void (*onKeyCb)(int x, int y, int z, void*) = nullptr;
  void* onKeyData = nullptr;

  using quadDataType = uint8_t[64];

public:
  bool connected = false;
  int rows;
  int cols;
  int nQuads;
  quadDataType quadData[4];
  uint8_t quadDirtyFlags;
  void init() {
    memset(&this->quadData[0][0], 0, sizeof(quadDataType) * 4);
    this->quadDirtyFlags = 0xFF;
  };

  void setQuadDirty(int quadIndex) {
    this->quadDirtyFlags |= (1 << quadIndex);
  }

  bool isQuadDirty(int quadIndex) {
    return this->quadDirtyFlags & (1 << quadIndex);
  }

  void setOnKey(void (*cb)(int x, int y, int z, void*), void* data) {
    this->onKeyCb = cb;
    this->onKeyData = data;
  }

  void setOnConnect(ConnectCallbackT cb, void* data) {
    this->onConnectCb = cb;
    this->onConnectData = data;
  }

  void led(int x, int y, int c) {
    const auto [quadIndex, pointIndex] = calcQuadIndexAndPointIndex(x, y);
    quadData[quadIndex][pointIndex] = c;
    this->setQuadDirty(quadIndex);
  }

  void clear() {
    memset(&quadData, 0, sizeof(quadDataType) * 4);
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
        this->sendRawQuadMap(offX, offY, quadData[quadIndex]);
      }
    }
    this->quadDirtyFlags = 0;
  }

  virtual void sendRawQuadMap(int offX, int offY, quadDataType data) = 0;
  virtual void connect() = 0;
};

#ifdef __linux__

#include "OscInterface.hpp"


static void monome_grid_lo_on_error_func(int num, const char *msg, const char *path)
{
  printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

static void monome_grid_lo_server_fd_ready_callback(int fd, void* userData);
static int monome_grid_lo_generic_osc_callback(const char *path, const char *types,
                    lo_arg ** argv,
                    int argc, lo_message data, void *user_data);

static const int gridOscServerPort = 7072;

class LinuxMonomeGridInterface : public MonomeGridInterface {
public:
  using oscAddrT = LinuxOscInterface::address_type;
  oscAddrT serialoscdAddr;
  oscAddrT gridAddr;
  LinuxOscInterface oscInterface;
  std::string gridId;
  bool autoReconnect = true;
  void init(LinuxPlatform* platform) {
    MonomeGridInterface::init();
    oscInterface.init(platform);
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

  void sendRawQuadMap(int offX, int offY, quadDataType data) override {
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

  void connect() override {
    this->oscInterface.sendMessage(this->serialoscdAddr, "/serialosc/list", "si",
                                   "localhost", this->oscInterface.port, LO_ARGS_END);
  }

  void uponOscMessage(const char *path, const char *types,
                      lo_arg ** argv,
                      int argc, lo_message data) {
    if (strcmp(path, "/monome/grid/key") == 0) {
      int x = argv[0]->i;
      int y = argv[1]->i;
      int z = argv[2]->i;
      if (this->onKeyCb) {
        this->onKeyCb(x, y, z, this->onKeyData);
      }
    } else if (strcmp(path, "/serialosc/device") == 0) {
      if (this->connected) { return; }
      int gridPort = argv[2]->i;
      gridId = &argv[0]->s;
      // printf("%s\n", &argv[1]->s); => "monome zero"
      this->gridAddr = this->oscInterface.newAddress(gridPort);
      this->oscInterface.sendMessage(this->gridAddr, "/sys/port", "i", this->oscInterface.port, LO_ARGS_END);
      this->oscInterface.sendMessage(this->gridAddr, "/sys/prefix", "s", "/monome", LO_ARGS_END);
      this->oscInterface.sendMessage(this->gridAddr, "/sys/info", NULL, LO_ARGS_END);   
    } else if (strcmp(path, "/sys/size") == 0) {
      this->rows = argv[0]->i;
      this->cols = argv[1]->i;
      this->nQuads = (this->rows / 8) * (this->cols / 8);
      this->connected = true;
      if (this->onConnectCb) {
        this->onConnectCb(this->gridId, true, this->onConnectData);
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
      this->connect();
    }
    requestDeviceNotifications();
  }

  void requestDeviceNotifications() {
    this->oscInterface.sendMessage(this->serialoscdAddr, "/serialosc/notify",
                                   "si", "localhost", this->oscInterface.port, LO_ARGS_END);
    
  }

  void disconnect(bool manual = true) {
    if (connected) {
      this->oscInterface.freeAddress(this->gridAddr);
      this->gridAddr = nullptr;
      this->connected = false;
      if (manual) {
        this->autoReconnect = false;
      }
      if (this->onConnectCb) {
        this->onConnectCb(this->gridId, false, this->onConnectData);
      }
    }
  }

  void cleanup() {
    disconnect();
    this->oscInterface.freeAddress(this->serialoscdAddr);
    this->oscInterface.cleanup();
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




#endif
