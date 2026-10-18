#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>

#include "MonomeGridInterface.hpp"
#include "OscInterface.hpp"

namespace pallet {

namespace grid::implementation {

using QuadType = uint8_t[64];

template <class InterfaceType, class IdType, class Traits>
struct IdWrapper {
  InterfaceType* iface;
  IdType id;
  
  IdWrapper(InterfaceType* iface, IdType id) : iface{iface}, id{id + 1} {}
  IdWrapper() : iface{nullptr}, id{0} {}
  IdWrapper(IdWrapper&& other) : iface{std::exchange(other.iface, nullptr)},
                             id{std::exchange(other.id, 0)} {}
  IdWrapper& operator=(IdWrapper&& other) {
    if (this != &other) {
      disconnect();
      iface = std::exchange(other.iface, nullptr);
      id = std::exchange(other.id, 0);
    }
    return *this;
  }
  
  operator bool() const {
    return !(id == 0);
  }
  IdType getId() const {
    return id - 1;
  }

  InterfaceType* getInterface() const {
    return iface;
  }

  void disconnect() {
    if (*this) {
      Traits::cleanup(iface, getId());
      this->id = 0;
    }
  }
  
  ~IdWrapper() {
    disconnect();
  }
};

struct OscAddressTraits {
  static inline void cleanup(OscInterface* iface, OscInterface::AddressId id) {
    iface->freeAddress(id);
  }
};

struct OscAddress : public IdWrapper<OscInterface, OscInterface::AddressId, OscAddressTraits> {
  using IdWrapper::IdWrapper;

  static Result<OscAddress> create(OscInterface* iface, int port) {
    return iface->createAddress(port).transform([&](OscInterface::AddressId id) {
      return OscAddress(iface, id);
    });
  }
  
};


struct OscServerTraits {
  static inline void cleanup(OscInterface* iface, OscInterface::ServerId id) {
    iface->freeServer(id);
  }
};

struct OscServer : public IdWrapper<OscInterface, OscInterface::ServerId, OscServerTraits> {
  using IdWrapper::IdWrapper;

  static Result<OscServer> create(OscInterface* iface, int port) {
    return iface->createServer(port).transform([&](OscInterface::ServerId id) {
      return OscServer(iface, id);
    });
  }

};


struct LEDState {
  QuadType data[4] = {};
  uint8_t dirty = 0xFF;
};

// Everything known about one serialosc device. Devices are never
// removed from the table, so a device's position is its stable
// MonomeGridInterface::Id.
struct Device {
  enum class Phase {
    Probing,    // waiting for the /sys/info reply
    Ready,      // properties known, nobody has connected to it
    Connected,  // connected, and the device is present
    Gone,       // unplugged, or about to be re-probed
  };

  std::string id;
  std::string type;
  OscAddress addr;
  int rows = 0;
  int cols = 0;
  int rotation = 0;
  Phase phase = Phase::Probing;
  // set by connect(), kept across unplugging so that the grid is
  // re-activated when it comes back
  bool claimed = false;
  LEDState ledState;
  Event<void(int, int, int)> onKey;
};

}

struct OscMonomeGridInterface final : public MonomeGridInterface {
public:

  using GridIndex = MonomeGridInterface::Id;

  static Result<OscMonomeGridInterface> create(OscInterface& oscInterface);
  // the OSC listener captures this
  OscMonomeGridInterface(const OscMonomeGridInterface&) = delete;
  OscMonomeGridInterface& operator=(const OscMonomeGridInterface&) = delete;

  OscMonomeGridInterface(OscInterface& iface, grid::implementation::OscAddress serialoscdAddr,
                         grid::implementation::OscServer oscServer, int serverPort);

  // the number of grids serialosc has reported so far, which is also
  // one more than the largest valid GridIndex
  size_t deviceCount() const { return devices.size(); }

private:
  // the handling of the serialosc messages, defined in the .cpp
  struct Protocol;

  OscInterface* oscInterface;
  grid::implementation::OscAddress serialoscdAddr;
  grid::implementation::OscServer oscServer;
  // the port that oscServer actually listens on, chosen by the system
  int serverPort;

  // index == GridIndex; a deque so that references stay valid when
  // devices are added
  std::deque<grid::implementation::Device> devices;
  std::unordered_map<std::string, GridIndex> devicesById;
  // connect() requests for devices that are not ready yet (or not
  // discovered yet)
  std::unordered_map<GridIndex, MonomeGridInterface::OnConnectCallback> pendingConnections;
  // connect() requests by serial for devices that are not discovered
  // yet; moved to pendingConnections once the device gets an index
  std::unordered_map<std::string, MonomeGridInterface::OnConnectCallback> pendingSerialConnections;
  // Devices waiting for properties. Replies to /sys/info carry no
  // sender, so only the front device is queried and replies belong to it.
  std::deque<GridIndex> probeQueue;

  virtual void connectImpl(Id idx, OnConnectCallback) override;
  virtual void connectBySerialImpl(std::string_view serial, OnConnectCallback) override;
  virtual void ledImpl(Id id, int x, int y, int c) override;
  virtual void allImpl(Id id, int c) override;
  virtual void clearImpl(Id id) override;
  virtual void renderImpl(Id id) override;
  virtual int getRowsImpl(Id id) const override;
  virtual int getColsImpl(Id id) const override;
  virtual int getRotationImpl(Id id) const override;
  virtual const char* getIdImpl(Id id) const override;
  virtual bool isConnectedImpl(Id id) const override;
  virtual KeyEventId listenImpl(Id id, Callable<void(int, int, int)>) override;
  virtual void unlistenImpl(Id id, KeyEventId) override;
};

}
