#include "pallet/OscMonomeGridInterface.hpp"
#include <cstdio>
#include <system_error>
#include <charconv>
#include <cstring>
#include <algorithm>
#include <optional>
#include <tuple>

namespace pallet {

using grid::implementation::Device;
using grid::implementation::LEDState;
using grid::implementation::OscAddress;
using grid::implementation::OscServer;
using grid::implementation::QuadType;

using GridIndex = OscMonomeGridInterface::GridIndex;

static constexpr const int32_t SERIALOSCD_PORT = 12002;


static inline int calcQuadIndex(int x, int y) {
  return (x / 8) + (y / 8) * 2;
}

static inline std::pair<int, int> calcQuadIndexAndPointIndex(int x, int y) {
  return std::pair(calcQuadIndex(x, y), (x % 8) + 8 * (y % 8));
}

static void setDirty(LEDState& m, size_t i) {
  m.dirty |= (1 << i);
}

static bool isDirty(LEDState& m, size_t i) {
  return (bool)(m.dirty & (1 << i));
}

static void clearDirty(LEDState& m) {
  m.dirty = 0;
}

static void led(LEDState& m, int x, int y, int c) {
  auto [quadIndex, pointIndex] = calcQuadIndexAndPointIndex(x, y);
  m.data[quadIndex][pointIndex] = c;
  setDirty(m, quadIndex);
};

static void all(LEDState& m, int c) {
  memset(&m.data, c, sizeof(QuadType) * 4);
  for (int i = 0; i < 4; i++) {
    setDirty(m, i);
  } 
}

static void clear(LEDState& m) {
  all(m, 0);
}

constinit const size_t N_QUAD_VALUES = sizeof(QuadType)/sizeof(uint8_t);

static void sendQuad(OscInterface* iface, const OscAddress& addr, const LEDState& ledState, int quadIndex, int offX, int offY) {
  ([&]<size_t... indexes>(std::index_sequence<indexes...>){
    iface->send(addr.getId(),
                "/monome/grid/led/level/map",
                offX, offY, (static_cast<int32_t>(ledState.data[quadIndex][indexes]))...);
  })(std::make_index_sequence<N_QUAD_VALUES>{});
}

// Returns nothing unless there are exactly sizeof...(Types) items
// and each one has the requested type. OSC input is untrusted.
template <class... Types>
static std::optional<std::tuple<Types...>> extractOscValues(const OscItem* items, size_t n) {
  if (n != sizeof...(Types)) { return std::nullopt; }
  return [&]<size_t... index>(std::index_sequence<index...>) -> std::optional<std::tuple<Types...>> {
    auto pointers = std::make_tuple(pallet::get_if<Types>(items[index])...);
    if (!((std::get<index>(pointers) != nullptr) && ...)) { return std::nullopt; }
    return std::tuple<Types...>(*std::get<index>(pointers)...);
  }(std::index_sequence_for<Types...>{});
}


// The handling of the serialosc messages. It is a nested struct so that it
// can use the private state of the interface.
struct OscMonomeGridInterface::Protocol {

static Device* findDevice(OscMonomeGridInterface& iface, GridIndex id) {
  return id < iface.devices.size() ? &iface.devices[id] : nullptr;
}

static const Device* findDevice(const OscMonomeGridInterface& iface, GridIndex id) {
  return id < iface.devices.size() ? &iface.devices[id] : nullptr;
}

static void render(OscMonomeGridInterface& iface, GridIndex id) {
  Device* device = findDevice(iface, id);
  if (!device || device->phase != Device::Phase::Connected) { return; }
  auto& ledState = device->ledState;
  for (int quadIndex = 0; quadIndex < 4; quadIndex++) {
    int offX = (quadIndex % 2) * 8;
    int offY = (quadIndex / 2) * 8;
    // rotated grids can be 8 wide and 16 tall, so the quads that exist
    // are not always the first ones
    if (offX < device->cols && offY < device->rows && isDirty(ledState, quadIndex)) {
      sendQuad(iface.oscInterface, device->addr, ledState, quadIndex, offX, offY);
    }
  }
  clearDirty(ledState);
}

static void requestDeviceList(OscMonomeGridInterface& iface) {
  iface.oscInterface->send(iface.serialoscdAddr.getId(), "/serialosc/list",
                           "127.0.0.1", iface.serverPort);
}

// serialoscd sends add/remove notifications to a client only once
// per request, so this must be repeated after every notification
static void requestDeviceNotifications(OscMonomeGridInterface& iface) {
  iface.oscInterface->send(iface.serialoscdAddr.getId(), "/serialosc/notify",
                           "127.0.0.1", iface.serverPort);
}

static void probeFront(OscMonomeGridInterface& iface) {
  if (iface.probeQueue.empty()) { return; }
  const auto& device = iface.devices[iface.probeQueue.front()];
  iface.oscInterface->send(device.addr.getId(), "/sys/info", iface.serverPort);
}

static void probeDevice(OscMonomeGridInterface& iface, GridIndex idx) {
  iface.devices[idx].phase = Device::Phase::Probing;
  iface.probeQueue.push_back(idx);
  if (iface.probeQueue.size() == 1) { probeFront(iface); }
}

// the device was unplugged, so its replies no longer count
static void abandonProbe(OscMonomeGridInterface& iface, GridIndex idx) {
  bool wasFront = !iface.probeQueue.empty() && iface.probeQueue.front() == idx;
  std::erase(iface.probeQueue, idx);
  if (wasFront) { probeFront(iface); }
}

// Point the device at this interface and start using it.
static void activate(OscMonomeGridInterface& iface, GridIndex idx, bool redraw) {
  auto& device = iface.devices[idx];
  char prefix[16];
  snprintf(prefix, sizeof(prefix), "/p/%u", idx);
  iface.oscInterface->send(device.addr.getId(), "/sys/prefix", prefix);
  iface.oscInterface->send(device.addr.getId(), "/sys/host", "127.0.0.1");
  iface.oscInterface->send(device.addr.getId(), "/sys/port", iface.serverPort);
  device.phase = Device::Phase::Connected;
  device.claimed = true;
  if (redraw) {
    device.ledState.dirty = 0xFF;
    render(iface, idx);
  }
}

static void finishCallback(OscMonomeGridInterface& iface, GridIndex idx) {
  auto pending = iface.pendingConnections.extract(idx);
  if (pending) {
    std::move(pending.mapped())(idx);
  }
}

// All the properties of the device are known.
static void onDeviceProbed(OscMonomeGridInterface& iface, GridIndex idx) {
  auto& device = iface.devices[idx];
  if (iface.pendingConnections.contains(idx)) {
    activate(iface, idx, false);
    finishCallback(iface, idx);
  } else if (device.claimed) {
    // came back after being unplugged
    activate(iface, idx, true);
  } else {
    device.phase = Device::Phase::Ready;
  }
}

static void onDeviceInformation(OscMonomeGridInterface& iface,
                                std::string id,
                                std::string type,
                                int port) {
  auto it = iface.devicesById.find(id);
  if (it == iface.devicesById.end()) {
    auto addr = OscAddress::create(iface.oscInterface, port);
    // the port comes from the network
    if (!addr) { return; }
    GridIndex idx = iface.devices.size();
    auto& device = iface.devices.emplace_back();
    device.id = id;
    device.type = std::move(type);
    device.addr = std::move(*addr);
    auto waiting = iface.pendingSerialConnections.extract(device.id);
    iface.devicesById.emplace(std::move(id), idx);
    if (waiting) {
      iface.pendingConnections.emplace(idx, std::move(waiting.mapped()));
    }
    probeDevice(iface, idx);
    return;
  }
  GridIndex idx = it->second;
  auto& device = iface.devices[idx];
  if (device.phase == Device::Phase::Gone) {
    // plugged back in, possibly on a different port
    auto addr = OscAddress::create(iface.oscInterface, port);
    if (!addr) { return; }
    device.addr = std::move(*addr);
    probeDevice(iface, idx);
  }
  // otherwise this is a repeated listing of a device we already have
}

// serialoscd announces a device with the same (id, type, port) as in
// a /serialosc/device reply.
static void onDeviceChange(OscMonomeGridInterface& iface, std::string id, std::string type,
                           int port, bool added) {
  requestDeviceNotifications(iface);
  auto it = iface.devicesById.find(id);
  if (it != iface.devicesById.end()) {
    abandonProbe(iface, it->second);
    iface.devices[it->second].phase = Device::Phase::Gone;
    // the port may have changed; onDeviceInformation re-probes Gone devices
  }
  if (added) {
    onDeviceInformation(iface, std::move(id), std::move(type), port);
  }
  // events between a notification and the next /serialosc/notify are
  // lost, so resynchronise with the current list
  requestDeviceList(iface);
}

static void processOscMessages(OscMonomeGridInterface& iface, std::string_view path,
                               const OscItem* items, size_t n) {
  if (path.starts_with("/p/")) {
    // from a device!
    const char* start = path.data() + 3;
    const char* end = path.data() + path.size();
    unsigned int idx;
    auto result = std::from_chars(start, end, idx);
    if (result.ec != std::errc{}) { return; }
    if (std::string_view(result.ptr, end) != "/grid/key") { return; }
    auto values = extractOscValues<int32_t, int32_t, int32_t>(items, n);
    Device* device = findDevice(iface, idx);
    if (!values || !device || device->phase != Device::Phase::Connected) { return; }
    auto [x, y, z] = *values;
    device->onKey.emit(x, y, z);
  } else if (path == "/serialosc/device") {
    if (auto values = extractOscValues<std::string_view, std::string_view, int32_t>(items, n)) {
      auto [id, type, port] = *values;
      onDeviceInformation(iface, std::string(id), std::string(type), port);
    }
  } else if (path == "/serialosc/add" || path == "/serialosc/remove") {
    if (auto values = extractOscValues<std::string_view, std::string_view, int32_t>(items, n)) {
      auto [id, type, port] = *values;
      onDeviceChange(iface, std::string(id), std::string(type), port, path == "/serialosc/add");
    }
  }
  else if (path == "/sys/size" && !iface.probeQueue.empty()) {
    if (auto values = extractOscValues<int32_t, int32_t>(items, n)) {
      // serialosc sends the width first
      auto [cols, rows] = *values;
      // a grid has at most 16x16 keys, which is what LEDState stores
      if (rows > 0 && rows <= 16 && cols > 0 && cols <= 16) {
        iface.devices[iface.probeQueue.front()].rows = rows;
        iface.devices[iface.probeQueue.front()].cols = cols;
      }
    }
  } else if (path == "/sys/rotation" && !iface.probeQueue.empty()) {
    // the last reply to /sys/info
    if (auto values = extractOscValues<int32_t>(items, n)) {
      auto idx = iface.probeQueue.front();
      iface.devices[idx].rotation = std::get<0>(*values);
      iface.probeQueue.pop_front();
      onDeviceProbed(iface, idx);
      probeFront(iface);
    }
  }
}

};

int OscMonomeGridInterface::getRowsImpl(GridIndex id) const {
  const Device* device = Protocol::findDevice(*this, id);
  return device ? device->rows : 0;
}

int OscMonomeGridInterface::getColsImpl(GridIndex id) const {
  const Device* device = Protocol::findDevice(*this, id);
  return device ? device->cols : 0;
}

int OscMonomeGridInterface::getRotationImpl(GridIndex id) const {
  const Device* device = Protocol::findDevice(*this, id);
  return device ? device->rotation : 0;
}

const char* OscMonomeGridInterface::getIdImpl(GridIndex id) const {
  const Device* device = Protocol::findDevice(*this, id);
  return device ? device->id.c_str() : "";
}

bool OscMonomeGridInterface::isConnectedImpl(GridIndex id) const {
  const Device* device = Protocol::findDevice(*this, id);
  return device && device->phase == Device::Phase::Connected;
}

void OscMonomeGridInterface::ledImpl(GridIndex id, int x, int y, int c) {
  Device* device = Protocol::findDevice(*this, id);
  if (!device || x < 0 || y < 0 || x >= device->cols || y >= device->rows) { return; }
  pallet::led(device->ledState, x, y, c);
}

void OscMonomeGridInterface::allImpl(GridIndex id, int c) {
  if (Device* device = Protocol::findDevice(*this, id)) {
    pallet::all(device->ledState, c);
  }
}

void OscMonomeGridInterface::clearImpl(GridIndex id) {
  if (Device* device = Protocol::findDevice(*this, id)) {
    pallet::clear(device->ledState);
  }
}

MonomeGridInterface::KeyEventId OscMonomeGridInterface::listenImpl(GridIndex id, pallet::Callable<void(int, int, int)> func) {
  Device* device = Protocol::findDevice(*this, id);
  return device ? device->onKey.listen(std::move(func)) : 0;
}

void OscMonomeGridInterface::unlistenImpl(GridIndex id, MonomeGridInterface::KeyEventId eid) {
  if (Device* device = Protocol::findDevice(*this, id)) {
    device->onKey.unlisten(eid);
  }
}

void OscMonomeGridInterface::renderImpl(GridIndex id) {
  Protocol::render(*this, id);
}

Result<OscMonomeGridInterface> OscMonomeGridInterface::create(OscInterface& iface) {
  auto server = OscServer::create(&iface, 0);
  if (!server) { return pallet::error(server.error()); }
  // the system chooses the port
  auto serverPort = iface.serverPort(server->getId());
  if (!serverPort) { return pallet::error(serverPort.error()); }
  auto serialoscd = OscAddress::create(&iface, SERIALOSCD_PORT);
  if (!serialoscd) { return pallet::error(serialoscd.error()); }
  return Result<OscMonomeGridInterface>(std::in_place, iface, std::move(*serialoscd),
                                        std::move(*server), *serverPort);
}

OscMonomeGridInterface::OscMonomeGridInterface(OscInterface& iface, OscAddress serialoscdAddr,
                                               OscServer oscServer, int serverPort)
  : oscInterface(&iface),
    serialoscdAddr(std::move(serialoscdAddr)),
    oscServer(std::move(oscServer)),
    serverPort(serverPort) {

  // the server was created by the caller, so its id is live
  (void)oscInterface->listen(this->oscServer.getId(), [&](const char* path, const OscItem* items, size_t n) {
    Protocol::processOscMessages(*this, path, items, n);
  });

  Protocol::requestDeviceNotifications(*this);
  Protocol::requestDeviceList(*this);
}

void OscMonomeGridInterface::connectImpl(GridIndex idx, OnConnectCallback func) {
  Device* device = Protocol::findDevice(*this, idx);
  if (device && device->claimed) {
    std::move(func)(pallet::error(std::make_error_condition(std::errc::device_or_resource_busy)));
  } else if (device && device->phase == Device::Phase::Ready) {
    Protocol::activate(*this, idx, false);
    std::move(func)(idx);
  } else if (!pendingConnections.contains(idx)) {
    // not discovered, still being probed, or unplugged: finish when
    // the device is ready
    pendingConnections.emplace(idx, std::move(func));
  } else {
    std::move(func)(pallet::error(std::make_error_condition(std::errc::device_or_resource_busy)));
  }
}

void OscMonomeGridInterface::connectBySerialImpl(std::string_view serial, OnConnectCallback func) {
  std::string key(serial);
  auto it = devicesById.find(key);
  if (it != devicesById.end()) {
    connectImpl(it->second, std::move(func));
  } else if (pendingSerialConnections.contains(key)) {
    std::move(func)(pallet::error(std::make_error_condition(std::errc::device_or_resource_busy)));
  } else {
    pendingSerialConnections.emplace(std::move(key), std::move(func));
  }
}

}
