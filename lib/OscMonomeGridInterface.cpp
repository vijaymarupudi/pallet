#include "pallet/OscMonomeGridInterface.hpp"

namespace pallet {

static const int gridOscServerPort = 7072;

template <class Type>
static decltype(auto) get_unchecked(auto&& variant) {
  return variant.template get_unchecked<Type>();
}

  template <class... Types>
  std::tuple<Types...> extractOscValues(const OscItem* items) {
    auto tmp = ([&]<class... T, size_t... index>(std::index_sequence<index...>) {
        return std::make_tuple(get_unchecked<T>(items[index])...);
    });

    return tmp.template operator()<Types...>(std::make_index_sequence<sizeof...(Types)>{});
  }

Result<OscMonomeGridInterface> OscMonomeGridInterface::create(OscInterface& oscInterface) {
  return Result<OscMonomeGridInterface>(std::in_place_t{}, oscInterface);
}

OscMonomeGridInterface::OscMonomeGridInterface(OscInterface& ioscInterface)
  : oscInterface(&ioscInterface) {
  serialoscdAddr = oscInterface->createAddress(12002);
  oscInterface->onMessage.listen([&](const char *path, const OscItem* items,
                                     size_t n) {
                                   this->uponOscMessage(path, items, n);
                                 });
  this->oscInterface->bind(gridOscServerPort);
  requestDeviceNotifications();
}

void OscMonomeGridInterface::sendRawQuadMap(int offX, int offY, MonomeGrid::QuadType data) {

  auto tmp = [&]<size_t... indexes>(std::index_sequence<indexes...>){
    this->oscInterface->send(this->gridAddr,
                            "/monome/grid/led/level/map", offX, offY, (static_cast<int32_t>(data[indexes]))...);
  };

  tmp(std::make_index_sequence<64>{});
  // this->oscInterface->sendMessage(this->gridAddr, "/monome/grid/led/level/map", msg);
}

void OscMonomeGridInterface::connect(int id) {
  (void)id;
  this->oscInterface->send(this->serialoscdAddr, "/serialosc/list",
                          "localhost", this->oscInterface->port);
}

void OscMonomeGridInterface::uponOscMessage(const char *path, const OscItem* items,
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

    this->gridAddr = this->oscInterface->createAddress(gridPort);

    // set port, set prefix, ask for information

    this->oscInterface->send(this->gridAddr, "/sys/port", this->oscInterface->port);
    this->oscInterface->send(this->gridAddr, "/sys/prefix", "/monome");
    this->oscInterface->send(this->gridAddr, "/sys/info");

  } else if (strcmp(path, "/sys/size") == 0) {
    auto [rows, cols] = extractOscValues<int32_t, int32_t >(items);

    auto renderFunc = [](int offX, int offY, uint8_t* data, void* ud0, void* ud1) {
      (void)ud1;
      auto thi = static_cast<OscMonomeGridInterface*>(ud0);
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

void OscMonomeGridInterface::uponDeviceChange(const char* cStrId, bool addition) {
  std::string deviceId = cStrId;
  if (!addition && deviceId == this->gridId) {
    this->disconnect(false);
  }

  if (addition && deviceId == this->gridId && this->autoReconnect) {
    this->connect(0);
  }
  this->requestDeviceNotifications();
}

void OscMonomeGridInterface::requestDeviceNotifications() {
  this->oscInterface->send(this->serialoscdAddr, "/serialosc/notify",
                          "localhost", this->oscInterface->port);

}

void OscMonomeGridInterface::disconnect(bool manual) {
  if (grid && grid->isConnected()) {
    this->oscInterface->freeAddress(this->gridAddr);
    MonomeGridInterface::uponConnectionState(*this->grid, false);
    if (manual) {
      this->autoReconnect = false;
    }
    if (this->onConnectCb) {
      this->onConnectCb(this->gridId, false, nullptr, this->onConnectData);
    }
  }
}

OscMonomeGridInterface::~OscMonomeGridInterface() {
  this->disconnect();
  this->oscInterface->freeAddress(this->serialoscdAddr);
}

}
