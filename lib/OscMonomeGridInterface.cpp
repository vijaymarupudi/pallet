#include "pallet/OscMonomeGridInterface.hpp"
#include <cstdio>
#include <system_error>
#include <charconv>

namespace pallet {

static const int GRID_OSC_SERVER_PORT = 7072;
static constexpr const int SERIALOSCD_PORT = 12002;


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
    dirty(m, i);
  } 
}

static void clear(LEDState& m) {
  all(m, 0);
}

static void sendQuad(OscInterface* iface, const OscAddress& addr, const LEDState& ledState, int quadIndex, int offX, int offY) {
  ([&]<size_t... indexes>(std::index_sequence<indexes...>){
    iface->send(addr.getId(),
                "/monome/grid/led/level/map",
                offX, offY, (static_cast<int32_t>(ledState.data[quadIndex][indexes]))...);
  })(std::make_index_sequence<sizeof(QuadType)/sizeof(*QuadType)>{});
}

static void render(OscMonomeGridInterface& iface, GridIndex id) {
  auto& gridState = iface.gridStates[id];
  if (!gridState.connected) { return; }
  auto& ledState = gridState.ledState;
  for (int quadIndex = 0; quadIndex < gridState.metadata.nQuads; quadIndex++) {
    if (isDirty(ledState, quadIndex)) {
      int offX = (quadIndex % 2) * 8;
      int offY = (quadIndex / 2) * 8;
      sendQuad(iface.oscInterface, iface.gridsInformation[id].addr, ledState, quadIndex, offX, offY);
    }
  }
  clearDirty(ledState);
}


template <class Type>
static decltype(auto) get_unchecked(auto&& variant) {
  return std::forward<decltype(variant)>(variant).template get_unchecked<Type>();
}

template <class... Types>
std::tuple<Types...> extractOscValues(const OscItem* items) {
  auto tmp = ([&]<class... T, size_t... index>(std::index_sequence<index...>) {
      return std::make_tuple(get_unchecked<T>(items[index])...);
    });

  return tmp.template operator()<Types...>(std::make_index_sequence<sizeof...(Types)>{});
}

/*
 * NEW
 */

int OscMonomeGridInterface::getRowsImpl(GridIndex id) const {
  return gridsInformation[id].rows;
}

int OscMonomeGridInterface::getColsImpl(GridIndex id) const {
  return gridsInformation[id].cols;
}

int OscMonomeGridInterface::getRotationImpl(GridIndex id) const {
  return gridsInformation[id].rotation;
}

const char* OscMonomeGridInterface::getIdImpl(GridIndex id) const {
  return gridsInformation[id].id.c_str();
}

bool OscMonomeGridInterface::isConnectedImpl(GridIndex id) const {
  auto it = gridStates.find(id);
  if (it != gridStates.end()) {
    return (*it).second.connected;
  }

  return false;
  
}

void OscMonomeGridInterface::ledImpl(GridIndex id, int x, int y, int c) {
  return led(gridStates[id].ledState, x, y, c);
}

void OscMonomeGridInterface::allImpl(GridIndex id, int c) {
  return all(gridStates[id].ledState, c);
}

void OscMonomeGridInterface::clearImpl(GridIndex id) {
  return clear(gridStates[id].ledState);
}

uint32_t OscMonomeGridInterface::listenOnKeyImpl(GridIndex id, pallet::Callable<void(int, int, int)> func) {
  return gridStates[id].onKey.listen(std::move(func));
}

uint32_t OscMonomeGridInterface::unlistenOnKeyImpl(GridIndex id, uint32_t eid) {
  return gridStates[id].onKey.unlisten(eid);
}

void OscMonomeGridInterface::renderImpl(GridIndex id) {
  render(*this, id);
}

void enumerateDevices(OscMonomeGridInterface& iface) {
  iface.oscInterface->send(iface.serialoscdAddr.getId(), "/serialosc/list",
                           "localhost", iface.oscServer.getId());
}

void retrieveDeviceProperties(OscMonomeGridInterface& iface) {
  if (devicePropertiesProcessingQueue.size() == 0) { return; }
  auto& info = iface.devicePropertiesProcessingQueue.front();
  iface.oscInterface->send(info.addr.getId(), "/sys/info", GRID_OSC_SERVER_PORT);
}

static void onDeviceChange(OscMonomeGridInterface& iface, std::string id, bool connected) {
  auto it = gridsInformationIdxById.find(id)
    if (it != gridsInformationIdxById.end()) {
      auto idx = *it;
      auto& info = gridsInformation[idx];
      if (connected) {
        info.connected = true;
        if (!info.new_) {
          // reconnecting, render again
          iface.render(idx);
        }
      } else {
        info.connected = false;
      }
    } else {
      // need to learn more about this one, so enumerate everything
      // again
      enumerateDevices(iface);
    }
}


static void processConnectRequests(OscMonomeGridInterface& iface);

void processOscMessages(OscMonomeGridInterface& iface, std::string_view path, const OscItem* items) {
  if (path.starts_with("/p/")) {
    // from a device!
    const char* start = path.data() + 3;
    const char* end = path.data() + path.size();
    unsigned int idx;
    auto result = from_chars(start, end, val);
    auto command = std::string_view(results.ptr, end);
    if (command == "/grid/key") {
      auto [x, y, z] = extractOscValues<int32_t, int32_t, int32_t>(items);
      gridStates[idx].onKey.emit(x, y, z);
    }
  } else if (path == "/serialosc/device") {
    auto& [id, type, port] = extractOscValues<std::string_view, std::string_view, int32_t>(items);
    onDeviceInformation(iface, id, type, port);
  } else if (path == "/serialosc/add") {
    auto& [id] = extractOscValues<std::string_view>(items);
    onDeviceChange(iface, id, true);
  } else if (path == "/serialosc/remove") {
    auto& [id] = extractOscValues<std::string_view>(items);
    onDeviceChange(iface, id, false);
  }
  // these are device properties
  else if (path == "/sys/size") {
    auto [rows, cols] = extractOscValues<int32_t, int32_t >(items);
    auto& info = iface.devicePropertiesProcessingQueue.front();
    info.rows = rows;
    info.cols = cols;
  } else if (path == "/sys/host") {
    auto [host] = extractOscValues<std::string_view>(items);
    auto& info = iface.devicePropertiesProcessingQueue.front();
    info.host = std::move(host);
  } else if (path == "/sys/prefix") {
    auto [prefix] = extractOscValues<std::string_view>(items);
    auto& info = iface.devicePropertiesProcessingQueue.front();
    info.prefix = std::move(prefix);
  } else if (path == "/sys/rotation") {
    // This is the final property
    auto [rotation] = extractOscValues<int32_t>(items);
    auto&& info = std::move(iface.devicePropertiesProcessingQueue.front());
    iface.devicePropertiesProcessingQueue.pop();
    info.rotation = rotation;
    info.index = iface.gridsInformation.size();
    iface.gridsInformation.push_back(std::move(info));
    processConnectRequests(iface);
    // if more are needed
    retrieveDeviceProperties(iface);
  }
}

Result<OscMonomeGridInterface> OscMonomeGridInterface::create(OscInterface& iface) {
  return Result<OscMonomeGridInterface>(std::in_place,
                                        iface,
                                        OscAddress::create(&iface, SERIALOSCD_PORT),
                                        OscServer::create(&iface, GRID_OSC_SERVER_PORT));
  
}

OscMonomeGridInterface::OscMonomeGridInterface(OscInterface& iface, OscAddress serialoscdAddr,
                                               OscServer oscServer)
  : iface(&iface),
    serialoscdAddr(std::move(serialoscdAddr)),
    oscServer(std::move(oscServer)) {
  
  iface.listen(server.getId(), [&](const char* path, const OscItem* items, size_t n) {
    processOscMessages(*this, path, items);
  });
}

static void onDeviceInformation(OscMonomeGridInterface& iface,
                         std::string id,
                         std::string type,
                         int port) {
  if (gridsInformationIdxById.contains(id)) { return; }
  else {
    auto info = GridInfo{
      .id = std::move(id),
      .type = std::move(type),
      .port = port,
      .addr = OscAddress::create(iface.oscInterface, port)
    };
    devicePropertiesProcessingQueue.emplace_back(std::move(info));
    retrieveDeviceProperties(iface);
  }
}




static void connect(OscMonomeGridInterface& iface, GridIndex idx) {
  auto& info = iface.gridsInformation(idx);
  auto port = info.port;
  
  if (info.new_) {
    gridStates[idx] = GridState{};
    memset(&gridStates[idx].ledState.data, 0, sizeof(gridStates[idx].ledState.data));
  }

  char buf = buf[16];
  snprintf(buf, 16, "/p/%d", idx);
  iface.oscInterface->send(iface.gridsInformation[idx].addr.getId(), "/sys/prefix", buf);
  auto& state = gridStates[idx];
  info.new_ = false;
  info.connected = true;
  auto callback = std::move(pendingConnections[idx]);
  std::move(callback)(idx);
  pendingConnections.erase(idx);
}

static void processConnectRequests(OscMonomeGridInterface& iface) {
  for (const auto& [idx, connectRequest] : iface.pendingConnections) {
    auto it = gridsInformation.find(idx);
    if (it != gridsInformation.end()) {
      if (!(*it).connected) {
        connect(iface, idx);  
      } else {
        auto callback = std::move(pendingConnections[idx]);
        std::move(callback)(pallet::error(std::errc::make_error_condition(
                                            std::errc::device_or_resource_busy)));
        pendingConnections.erase(it);
      }
    }
  }
}

void OscMonomeGridInterface::connectImpl(int idx, OnConnectCallback func) {
  pendingConnections[idx] = std::move(func);
  processConnectRequests(*this);
}


}
