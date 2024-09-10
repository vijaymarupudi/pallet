#include "Platform.hpp"
#include <utility>
#include <string.h>
#ifdef __linux__
#include "lo/lo.h"
#endif

static int calcQuadIndex(int x, int y) {
  return (x / 8) + (y / 8) * 2;
}

static std::pair<int, int> calcQuadIndexAndPointIndex(int x, int y) {
  return std::pair(calcQuadIndex(x, y), (x % 8) + 8 * (y % 8));
}

class MonomeGridInterface {
protected:
  void (*onConnectCb)(void*) = nullptr;
  void* onConnectData = nullptr;

  void (*onKeyCb)(int x, int y, int z, void*) = nullptr;
  void* onKeyData = nullptr;

  using quadDataType = uint8_t[64];

public:
  Platform* platform;
  bool connected = false;
  int rows;
  int cols;
  int nQuads;
  quadDataType quadData[4];
  uint8_t quadDirtyFlags;
  virtual void init(Platform* platform) {
    this->platform = platform;
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

  void setOnConnect(void(*cb)(void*), void* data) {
    this->onConnectCb = cb;
    this->onConnectData = data;
  }

  void led(int x, int y, int c) {
    const auto [quadIndex, pointIndex] = calcQuadIndexAndPointIndex(x, y);
    quadData[quadIndex][pointIndex] = c;
    this->setQuadDirty(quadIndex);
  }

  void render() {
    if (!this->connected) { return; }
    for (int quadIndex = 0; quadIndex < this->nQuads; quadIndex++) {
      if (this->isQuadDirty(quadIndex)) {
        int offX = (quadIndex % 2) * 8;
        int offY = (quadIndex / 2) * 8;
        this->rawQuadMap(offX, offY, quadData[quadIndex]);
      }
    }
    this->quadDirtyFlags = 0;
  }

  virtual void rawQuadMap(int offX, int offY, quadDataType data) = 0;
  virtual void connect() = 0;
  
};

#ifdef __linux__

static void monome_grid_lo_on_error_func(int num, const char *msg, const char *path)
{
  printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

static void monome_grid_lo_server_fd_ready_callback(int fd, void* userData);
static int monome_grid_lo_generic_osc_callback(const char *path, const char *types,
                    lo_arg ** argv,
                    int argc, lo_message data, void *user_data);

static const char* osc_server_port_string = "7072";
static const int osc_server_port = 7072;

class LinuxMonomeGridInterface : public MonomeGridInterface {
private:
  
public:
  lo_server osc_server;
  lo_address serialoscd_addr;
  lo_address grid_addr;

  void init(Platform* platformin) override {
    MonomeGridInterface::init(platformin);
    auto platform = (LinuxPlatform*)platformin;
    serialoscd_addr = lo_address_new(NULL, "12002");
    this->osc_server = lo_server_new(osc_server_port_string, monome_grid_lo_on_error_func);
    lo_server_add_method(this->osc_server, NULL, NULL, monome_grid_lo_generic_osc_callback, this);
    int fd = lo_server_get_socket_fd(this->osc_server);
    platform->watchFd(fd, monome_grid_lo_server_fd_ready_callback, this);
  }

  void rawQuadMap(int offX, int offY, quadDataType data) override {
    lo_message msg = lo_message_new();
    lo_message_add_int32(msg, offX);
    lo_message_add_int32(msg, offY);
    for (int i = 0; i < 64; i++) {
      lo_message_add_int32(msg, data[i]);
    }
    lo_send_message(this->grid_addr, "/monome/grid/led/level/map", msg);
    lo_message_free(msg);
  }

  void connect() override {
    lo_send(serialoscd_addr, "/serialosc/list", "si", "localhost", osc_server_port);
  }

  void osc_response(const char *path, const char *types,
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
      int port = argv[2]->i;
      char buffer[16];
      snprintf(buffer, sizeof(buffer), "%d", port);
      this->grid_addr = lo_address_new(NULL, buffer);
      lo_send(this->grid_addr, "/sys/port", "i", osc_server_port);
      lo_send(this->grid_addr, "/sys/prefix", "s", "/monome");
      lo_send(this->grid_addr, "/sys/info", NULL);   
    } else if (strcmp(path, "/sys/size") == 0) {
      this->rows = argv[0]->i;
      this->cols = argv[1]->i;
      this->nQuads = (this->rows / 8) * (this->cols / 8);
      connected = true;
      if (onConnectCb) {
        this->onConnectCb(this->onConnectData);
      }
    }

  }
};

static int monome_grid_lo_generic_osc_callback(const char *path, const char *types,
                    lo_arg ** argv,
                    int argc, lo_message data, void *user_data) {
  ((LinuxMonomeGridInterface*)user_data)->osc_response(path, types, argv, argc, data);
  return 0;
}


static void monome_grid_lo_server_fd_ready_callback(int fd, void* userData) {
  auto server = ((LinuxMonomeGridInterface*)userData)->osc_server;
  lo_server_recv_noblock(server, 0);
}


#endif
