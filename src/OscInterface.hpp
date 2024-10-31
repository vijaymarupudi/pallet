#include "lo/lo.h"

static void osc_interface_lo_on_error_func(int num, const char *msg, const char *path)
{
  printf("liblo server error %d in path %s: %s\n", num, path, msg);
}

typedef void(*OscInterfaceCallback)(const char *path,
                                    const char *types,
                                    lo_arg ** argv,
                                    int argc, lo_message data,
                                    void *user_data);
class OscInterface {
  OscInterfaceCallback onMessageCb;
  OscInterfaceCallback onMessageUserData;
  void init() {
    onMessageCb = nullptr;
    onMessageUserData = nullptr;
  }
  virtual void bind(int port) = 0;
  void setOnMessage(OscInterfaceCallback cb, void* userData) {
    this->onMessageCb = cb;
    this->onMessageUserData = userData;
  }
  virtual void sendMessage(const char* addr, int port, lo_message msg) = 0;
  void cleanup() {}
};


static int monome_grid_lo_generic_osc_callback(const char *path, const char *types,
                    lo_arg ** argv,
                    int argc, lo_message data, void *user_data) {
  ((LinuxMonomeGridInterface*)user_data)->osc_response(path, types, argv, argc, data);
  return 0;
}

class LinuxOscInterface : public OscInterface {
public:
  lo_server server = nullptr;
  void bind(int port) override {
    char buf[16];
    snprintf(buf, 16, "%d", port);
    // serverAddr = lo_address_new(NULL, buf);
    server = lo_server_new(port, osc_interface_lo_on_error_func);
    lo_server_add_method(this->osc_server, NULL, NULL, monome_grid_lo_generic_osc_callback, this);
  }
}
