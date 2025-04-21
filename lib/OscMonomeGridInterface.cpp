#include "pallet/OscMonomeGridInterface.hpp"
#include <cstdio>
#include <system_error>
#include <charconv>
#include <cstring>

namespace pallet {

using GridIndex = OscMonomeGridInterface::GridIndex;

static const int32_t GRID_OSC_SERVER_PORT = 7072;
static constexpr const int32_t SERIALOSCD_PORT = 12002;


template <class K, class V>
static V* unsafeLookupPointer(std::unordered_map<K, V>& map, auto&& idx) {
  auto end = map.end();
  auto it = map.find(std::forward<decltype(idx)>(idx));
  if (it != end) {
    return &((*it).second);
  } else {
    std::unreachable();
    // return nullptr;
  }
}

template <class K, class V>
static V& unsafeLookup(std::unordered_map<K, V>& map, auto&& idx) {
  return *unsafeLookupPointer(map, std::forward<decltype(idx)>(idx));
}

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

static void render(OscMonomeGridInterface& iface, GridIndex id) {
  auto& gridState = unsafeLookup(iface.gridsStates, id);
  if (!gridState.connected) { return; }
  auto& ledState = gridState.ledState;
  for (int quadIndex = 0; quadIndex < gridState.nQuads; quadIndex++) {
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
  auto it = gridsStates.find(id);
  if (it != gridsStates.end()) {
    return (*it).second.connected;
  }

  return false;
  
}

void OscMonomeGridInterface::ledImpl(GridIndex id, int x, int y, int c) {
  return pallet::led(unsafeLookup(gridsStates, id).ledState, x, y, c);
}

void OscMonomeGridInterface::allImpl(GridIndex id, int c) {
  return pallet::all(unsafeLookup(gridsStates, id).ledState, c);
}

void OscMonomeGridInterface::clearImpl(GridIndex id) {
  return pallet::clear(unsafeLookup(gridsStates, id).ledState);
}

MonomeGridInterface::KeyEventId OscMonomeGridInterface::listenImpl(GridIndex id, pallet::Callable<void(int, int, int)> func) {
  return unsafeLookup(gridsStates, id).onKey.listen(std::move(func));
}

void OscMonomeGridInterface::unlistenImpl(GridIndex id, MonomeGridInterface::KeyEventId eid) {
  return unsafeLookup(gridsStates, id).onKey.unlisten(eid);
}

void OscMonomeGridInterface::renderImpl(GridIndex id) {
  pallet::render(*this, id);
}

void enumerateDevices(OscMonomeGridInterface& iface) {
  iface.oscInterface->send(iface.serialoscdAddr.getId(), "/serialosc/list",
                           "localhost", (int32_t)iface.oscServer.getId());
}

void retrieveDeviceProperties(OscMonomeGridInterface& iface) {
  if (iface.devicePropertiesProcessingQueue.size() == 0) { return; }
  auto& info = iface.devicePropertiesProcessingQueue.front();
  iface.oscInterface->send(info.addr.getId(), "/sys/info", GRID_OSC_SERVER_PORT);
}

static void onDeviceInformation(OscMonomeGridInterface& iface,
                                std::string id,
                                std::string type,
                                int port) {
  if (iface.gridsInformationIdxById.contains(id)) { return; }
  else {
    auto info = GridInfo{};
    info.id = std::move(id);
    info.type = std::move(type);
    info.port = port;
    info.addr = OscAddress::create(iface.oscInterface, port);
    iface.devicePropertiesProcessingQueue.emplace(std::move(info));
    retrieveDeviceProperties(iface);
  }
}

static void onDeviceChange(OscMonomeGridInterface& iface, std::string id, bool connected) {
  auto it = iface.gridsInformationIdxById.find(id);
  if (it != iface.gridsInformationIdxById.end()) {
    auto idx = (*it).second;
    auto& state = unsafeLookup(iface.gridsStates, idx);
    if (connected) {
      state.connected = true;
      if (!state.new_) {
        // reconnecting, render again
        iface.render(idx);
      }
    } else {
      state.connected = false;
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
    auto result = std::from_chars(start, end, idx);
    auto command = std::string_view(result.ptr, end);
    if (command == "/grid/key") {
      auto [x, y, z] = extractOscValues<int32_t, int32_t, int32_t>(items);
      unsafeLookup(iface.gridsStates, idx).onKey.emit(x, y, z);
    }
  } else if (path == "/serialosc/device") {
    auto&& [id, type, port] = extractOscValues<std::string_view, std::string_view, int32_t>(items);
    onDeviceInformation(iface, std::string(id), std::string(type), port);
  } else if (path == "/serialosc/add") {
    auto&& [id] = extractOscValues<std::string_view>(items);
    onDeviceChange(iface, std::string(id), true);
  } else if (path == "/serialosc/remove") {
    auto&& [id] = extractOscValues<std::string_view>(items);
    onDeviceChange(iface, std::string(id), false);
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
  : oscInterface(&iface),
    serialoscdAddr(std::move(serialoscdAddr)),
    oscServer(std::move(oscServer)) {
  
  oscInterface->listen(this->oscServer.getId(), [&](const char* path, const OscItem* items, size_t n) {
    (void)n;
    processOscMessages(*this, path, items);
  });
}


static void connect(OscMonomeGridInterface& iface, GridIndex idx) {

  if (!iface.gridsStates.contains(idx)) {
    iface.gridsStates[idx] = GridState{};
    memset(&(unsafeLookup(iface.gridsStates, idx).ledState.data), 0,
           sizeof(QuadType));
  }

  char buf[16];
  snprintf(buf, 16, "/p/%d", idx);
  iface.oscInterface->send(iface.gridsInformation[idx].addr.getId(), "/sys/prefix", buf);
  auto& state = unsafeLookup(iface.gridsStates, idx);
  state.nQuads = 4;
  state.new_ = false;
  state.connected = true;
  
  auto callback = std::move(unsafeLookup(iface.pendingConnections, idx));
  std::move(callback)(idx);
  iface.pendingConnections.erase(idx);
}

static void processConnectRequests(OscMonomeGridInterface& iface) {
  for (const auto& [idx, connectRequest] : iface.pendingConnections) {
    if (idx < iface.gridsInformation.size()) {
      auto& state = unsafeLookup(iface.gridsStates, idx);
      if (!state.connected) {
        connect(iface, idx);  
      } else {
        auto callback = std::move(unsafeLookup(iface.pendingConnections, idx));
        std::move(callback)
          (pallet::error
           (std::make_error_condition(std::errc::device_or_resource_busy)));
        iface.pendingConnections.erase(idx);
      }
    }
  }
}

void OscMonomeGridInterface::connectImpl(GridIndex idx, OnConnectCallback func) {
  pendingConnections.emplace(idx, std::move(func));
  processConnectRequests(*this);
}


}
