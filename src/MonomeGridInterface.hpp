#pragma once

#include "Platform.hpp"
#include <utility>
#include <string.h>
#include <string>
#include <cstdint>
#include <optional>
#include "constants.hpp"
#include "error.hpp"

namespace pallet {

class MonomeGridInterface;

class MonomeGrid {
public:

  using QuadType = uint8_t[64];
  using QuadRenderFunction = void(*)(int offX, int offY, uint8_t* data,
                                     void* ud0, void* ud1);

  MonomeGrid(std::string id, int rows, int cols,
             QuadRenderFunction quadRenderFunc,
             void* quadRenderFuncUd0, void* quadRenderFuncUd1);

  void setOnKey(void (*cb)(int x, int y, int z, void*), void* data);

  bool isConnected();
  void led(int x, int y, int c);
  void all(int z);
  void clear();
  void render();
  int getRows();
  int getCols();

private:
  void (*onKeyCb)(int x, int y, int z, void*);
  void* onKeyData;

  bool connected = true;
  std::string id;
  int rows;
  int cols;
  int nQuads;

  QuadRenderFunction quadRenderFunc;
  void* quadRenderFuncUd0;
  void* quadRenderFuncUd1;

  QuadType quadData[4];
  uint8_t quadDirtyFlags = 0xFF;

  void setQuadDirty(int quadIndex);
  bool isQuadDirty(int quadIndex);
  void uponKey(int x, int y, int z);
  void uponConnectionState(bool state);
  friend class MonomeGridInterface;

};


class MonomeGridInterface {
  
public:

  using OnConnectCallback = void(*)(const std::string&, bool, MonomeGrid* grid, void*);

  void setOnConnect(OnConnectCallback cb, void* data) {
    this->onConnectCb = cb;
    this->onConnectData = data;
  }
  virtual void sendRawQuadMap(int offX, int offY, MonomeGrid::QuadType data) = 0;
  virtual void connect(int idx) = 0;

protected:
  
  OnConnectCallback onConnectCb = nullptr;
  void* onConnectData = nullptr;
  static void uponConnectionState(MonomeGrid&, bool);
  static void uponKey(MonomeGrid&, int x, int y, int z);

};

}

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

#include "LinuxOscInterface.hpp"

namespace pallet {

class LinuxMonomeGridInterface final : public MonomeGridInterface {
public:
  using OscAddressIdType = OscInterface::AddressIdType;
  
  static Result<LinuxMonomeGridInterface> create(LinuxPlatform& platform);
  LinuxMonomeGridInterface(LinuxPlatform& platform);
  void connect(int id) override;
  void disconnect(bool manual = true);
  ~LinuxMonomeGridInterface();

private:

  OscAddressIdType serialoscdAddr;
  OscAddressIdType gridAddr;
  LinuxOscInterface oscInterface;
  std::string gridId;
  std::optional<MonomeGrid> grid;
  bool autoReconnect = true;
  
  virtual void sendRawQuadMap(int offX, int offY, MonomeGrid::QuadType data) override;
  void uponOscMessage(const char *path, const OscItem* items, size_t n);
  void uponDeviceChange(const char* cStrId, bool addition);
  void requestDeviceNotifications();
  
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
