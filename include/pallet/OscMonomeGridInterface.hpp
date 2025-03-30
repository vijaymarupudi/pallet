#include <optional>

#include "MonomeGridInterface.hpp"
#include "OscInterface.hpp"

namespace pallet {

class OscMonomeGridInterface final : public MonomeGridInterface {
public:
  using OscAddressIdType = OscInterface::AddressIdType;
  
  static Result<OscMonomeGridInterface> create(OscInterface& oscInterface);
  OscMonomeGridInterface(OscInterface& oscInterface);
  void connect(int id) override;
  void disconnect(bool manual = true);
  ~OscMonomeGridInterface();

private:

  OscAddressIdType serialoscdAddr;
  OscAddressIdType gridAddr;
  OscInterface* oscInterface;
  std::string gridId;
  std::optional<MonomeGrid> grid;
  bool autoReconnect = true;
  
  virtual void sendRawQuadMap(int offX, int offY, MonomeGrid::QuadType data) override;
  void uponOscMessage(const char *path, const OscItem* items, size_t n);
  void uponDeviceChange(const char* cStrId, bool addition);
  void requestDeviceNotifications();
  
};

}

