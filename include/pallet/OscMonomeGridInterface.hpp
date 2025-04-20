#include <optional>
#include <string>
#include <unordered_map>
#include <queue>

#include "MonomeGridInterface.hpp"
#include "OscInterface.hpp"

namespace pallet {

// struct MonomeGridMetadata {
//   int rows;
//   int cols;
//   std::string id;
//   int rotation;
//   int nQuads;
// };

// class OscMonomeGridInterface final : public MonomeGridInterface {
// public:
//   using AddressId = OscInterface::AddressId;
//   using ServerId = OscInterface::ServerId;

//   static Result<OscMonomeGridInterface> create(OscInterface& oscInterface);
//   OscMonomeGridInterface(OscInterface& oscInterface);
//   void connect(int id) override;
//   void disconnect(bool manual = true);
//   ~OscMonomeGridInterface();

// private:

//   AddressId serialoscdAddr;
//   AddressId gridAddr;
//   ServerId oscServerId;

//   OscInterface* oscInterface;

//   std::string gridId;
//   std::optional<MonomeGrid> grid;
//   bool autoReconnect = true;

//   MonomeGridMetadata currentMetadata;

//   virtual void sendRawQuadMap(int offX, int offY, MonomeGrid::QuadType data) override;
//   void uponOscMessage(const char *path, const OscItem* items, size_t n);
//   void uponDeviceChange(const char* cStrId, bool addition);
//   void requestDeviceNotifications();

// };

namespace pallet::grid::implementation {

using QuadType = uint8_t[64];

template <class InterfaceType, class IdType, class Traits>
struct IdWrapper {
  InterfaceType* iface;
  IdType id;
  
  IdWrapper(InterfaceType* iface, IdType id) : iface{iface}, id{id + 1} {}
  IdWrapper() : iface{nullptr}, id{0} {}
  IdWrapper(IdWrapper&& other) : iface{std::exchange(other.iface, nullptr)},
                             id{std::exchange(other.id, 0)} {}
  IdWrapper& operator=(IdWrapper&& other) = default;
  
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

  static OscAddress create(OscInterface* iface, int port) {
    return OscAddress(iface, iface->createAddress(port));
  }
  
};


struct OscServerTraits {
  static inline void cleanup(OscInterface* iface, OscInterface::ServerId id) {
    iface->freeServer(id);
  }
};

struct OscServer : public IdWrapper<OscInterface, OscInterface::ServerId, OscServerTraits> {
  using IdWrapper::IdWrapper;

  static OscServer create(OscInterface* iface, int port) {
    return OscServer(iface, iface->createServer(port));
  }

};


// struct OscAddress {
//   OscInterface* iface;
//   OscInterface::AddressId address;
//   static OscAddress create(OscInterface* iface, int port) {
//     return OscAddress(iface, iface->createAddress(port));
//   }
//   OscAddress(OscInterface* iface, OscInterface::AddressId address) : iface{iface}, address{address + 1} {}
//   OscAddress() : iface{nullptr}, address{0} {}
//   OscAddress(OscAddress&& other) : iface{std::exchange(other.iface, nullptr)},
//                              address{std::exchange(other.address, 0)} {}
//   OscAddress& operator=(OscAddress&& other) = default;
  
//   operator bool() const {
//     return !(address == 0);
//   }
//   OscInterface::AddressId getId() const {
//     return address - 1;
//   }

//   void disconnect() {
//     if (*this) {
//       iface->freeAddress(getId());
//       this->address = 0;
//     }
//   }
  
//   ~OscAddress() {
//     disconnect();
//   }
// };
  




// struct OscServer {
//   OscInterface* iface;
//   OscInterface::ServerId server;
//   static OscServer create(OscInterface* iface, int port) {
//     return OscServer(iface, iface->createServer(port));
//   }
//   OscServer() : iface(nullptr), server(0) {}
//   OscServer(OscInterface* iface, OscInterface::ServerId iserver) : iface(iface), server(server + 1) {}
//   OscServer(OscServer&& other) : iface(std::exchange(other.iface, nullptr)),
//                                  server(std::exchange(other.server, 0)) {}
//   OscServer& operator=(OscServer&& other) = default;
//   operator bool() const {
//     return server != 0;
//   }
//   OscInterface::ServerId getId() const {
//     return server - 1;
//   }
//   void disconnect() {
//     iface->freeServer(getId());
//     this->server = 0;
//   }
//   ~OscServer() {
//     if (*this) {
//       disconnect();
//     }
//   }
// }


struct GridInfo {
  std::string id;
  std::string type;
  int port;
  OscAddress addr;
  std::string prefix;
  std::string host;
  int index;
  int rows;
  int cols;
  int rotation;
};

struct LEDState {
  QuadType data[4];
  uint8_t dirtyFlags = 0xFF;
};

struct GridState {
  bool connected = false;
  bool new_ = true;
  LEDState ledState;
  Event<void(int, int, int)> onKey;
};

};

using namespace pallet::grid::implementation;

struct OscMonomeGridInterface final : public MonomeGridInterface {
public:

  using AddressId = OscInterface::AddressId;
  using ServerId = OscInterface::ServerId;
  using GridIndex = MonomeGridInterface::Id;

  static Result<OscMonomeGridInterface> create(OscInterface& oscInterface);
  OscMonomeGridInterface(OscInterface& iface, OscAddress serialoscdAddr, OscServer oscServer);
  ~OscMonomeGridInterface();

  OscInterface* oscInterface;
  OscAddress serialoscdAddr;
  OscServer oscServer;
  

  std::unordered_map<GridIndex, GridState> gridStates;
  
  std::vector<GridInfo> gridsInformation;
  std::unordered_map<std::string, GridIndex> gridsInformationIdxById;
  std::queue<GridInfo> devicePropertiesProcessingQueue;
  std::unordered_map<GridIndex, MonomeGridInterface::OnConnectCallback> pendingConnections;

  virtual void connectImpl(int idx, OnConnectCallback) override;
  virtual void ledImpl(Id id, int x, int y, int c) override;
  virtual void allImpl(Id id, int c) override;
  virtual void clearImpl(Id id) override;
  virtual void renderImpl(Id id) override;
  virtual int getRowsImpl(Id id) const override;
  virtual int getColsImpl(Id id) const override;
  virtual int getRotationImpl(Id id) const override;
  virtual const char* getIdImpl(Id id) const override;
  virtual bool isConnectedImpl(Id id) const override;
  virtual KeyEventId listenOnKeyImpl(Id id, Callable<void(int, int, int)>) override;
  virtual void unlistenOnKeyImpl(Id id, KeyEventId) override;
};

}
