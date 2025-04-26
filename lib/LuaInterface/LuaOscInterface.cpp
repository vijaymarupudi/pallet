#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"
#include "../luaHelper/LightVariant.hpp"
#include "../luaHelper/vector.hpp"
#include "../luaHelper/LuaFunction.hpp"

using namespace pallet;
using namespace pallet::luaHelper;

namespace pallet::luaHelper {
template <>
struct LuaRetrieveContext<pallet::OscInterface&> {
  static inline pallet::OscInterface& retrieve(lua_State* L) {
    return *getLuaInterfaceObject(L).oscInterface;
  }
};
}

static void bindOscInterface(lua_State* L) {
  auto start = lua_gettop(L);
  getPalletCTable(L);
  auto palletCTableIndex = lua_gettop(L);
  lua_newtable(L); // osc
  int oscTableIndex = lua_gettop(L);
  rawSetTable(L, oscTableIndex, "createServer", [](OscInterface& iface, int port) {
    return iface.createServer(port);
  });

  rawSetTable(L, oscTableIndex, "freeServer", [](OscInterface& iface, OscInterface::ServerId serverId) {
    return iface.freeServer(serverId);
  });

  rawSetTable(L, oscTableIndex, "createAddress", [](OscInterface& iface, int port) {
    return iface.createAddress(port);
  });

  rawSetTable(L, oscTableIndex, "freeAddress", [](OscInterface& iface, OscInterface::AddressId addressId) {
    return iface.freeAddress(addressId);
  });

  rawSetTable(L, oscTableIndex, "listen", [](OscInterface& iface, OscInterface::ServerId server,
                                             luaHelper::LuaFunction func) {
    return iface.listen(server, [func = std::move(func)](const char* path, const OscItem* items, size_t n) mutable {
      func.call<void>(path, std::vector(items, items + n));
    });
  });

  rawSetTable(L, oscTableIndex, "unlisten", [](OscInterface& iface, OscInterface::ServerId server,
                                               OscInterface::MessageEvent::Id listener) {
    return iface.unlisten(server, listener);
  });

  rawSetTable(L, oscTableIndex, "send", [](OscInterface& iface, OscInterface::AddressId address,
                                           const std::string_view path, const std::vector<OscItem> items) {
    return iface.sendMessage(address, path.data(), items.data(), items.size());
  });

  lua_pushliteral(L, "osc");
  lua_pushvalue(L, oscTableIndex);
  lua_rawset(L, palletCTableIndex);
  lua_settop(L, start);
}


void LuaInterface::setOscInterface(OscInterface& oscInterface) {
  this->oscInterface = &oscInterface;
  bindOscInterface(this->L);
}
