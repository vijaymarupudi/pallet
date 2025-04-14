#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"

namespace pallet {

using namespace pallet::luaHelper;

  /*
 * Grid bindings
 */

  static int luaGridLed(lua_State* L) {
    auto [grid, x, y, z] = luaCheckedPullMultiple<
      MonomeGrid*, int, int, int>(L, 1);
    // auto grid = luaCheckedPull<MonomeGrid*>(L, 1);
    // int x = luaCheckedPull<int>(L, 2);
    // int y = luaCheckedPull<int>(L, 3);
    // int z = luaCheckedPull<int>(L, 4);
    grid->led(x - 1, y - 1, z);
    return 0;
  }

static int luaGridAll(lua_State* L) {
  auto grid = luaCheckedPull<MonomeGrid*>(L, 1);
  int z = luaCheckedPull<int>(L, 2);
  grid->all(z);
  return 0;
}

  static int luaGridClear(lua_State* L) {
    auto grid = luaCheckedPull<MonomeGrid*>(L, 1);
    grid->clear();
    return 0;
  }

  static int luaGridRender(lua_State* L) {
    auto grid = luaCheckedPull<MonomeGrid*>(L, 1);
    grid->render();
    return 0;
  }

  static int luaGridSetOnKey(lua_State* L) {
    auto grid = luaCheckedPull<MonomeGrid*>(L, 1);
    int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
    auto& iface = getLuaInterfaceObject(L);
    iface.gridKeyFunction = functionRef;
    grid->setOnKey([](int x, int y, int z, void* ud) {
      auto& luaInterface = *static_cast<LuaInterface*>(ud);
      lua_rawgeti(luaInterface.L, LUA_REGISTRYINDEX, luaInterface.gridKeyFunction);
      push(luaInterface.L, x + 1);
      push(luaInterface.L, y + 1);
      push(luaInterface.L, z);
      lua_call(luaInterface.L, 3, 0);
    }, &iface);
    return 0;
  }


  static int luaGridConnect(lua_State* L) {
    int id = luaCheckedPull<int>(L, -2);
    // connect callback is the first argument
    int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
    auto& iface = getLuaInterfaceObject(L);
    iface.gridOnConnectFunction = functionRef;
    iface.gridInterface->setOnConnect([](const std::string& id, bool state,
                                         MonomeGrid* grid, void* ud) {
      (void)id;
      (void)state;
      auto& luaInterface = *static_cast<LuaInterface*>(ud);
      lua_rawgeti(luaInterface.L, LUA_REGISTRYINDEX, luaInterface.gridOnConnectFunction);
      push(luaInterface.L, grid);
      lua_call(luaInterface.L, 1, 0);
    }, &iface);
    iface.gridInterface->connect(id);
    return 0;
  }

static void bindGrid(lua_State* L) {
  lua_newtable(L);
  int gridTableIndex = lua_gettop(L);
  luaRawSetTable(L, gridTableIndex, "connect", luaGridConnect);
  luaRawSetTable(L, gridTableIndex, "led", luaGridLed);
  luaRawSetTable(L, gridTableIndex, "clear", luaGridClear);
  luaRawSetTable(L, gridTableIndex, "all", luaGridAll);
  luaRawSetTable(L, gridTableIndex, "render", luaGridRender);
  luaRawSetTable(L, gridTableIndex, "refresh", luaGridRender);
  luaRawSetTable(L, gridTableIndex, "setOnKey", luaGridSetOnKey);
  getPalletCTable(L);
  int palletCTableIndex = lua_gettop(L);
  lua_pushliteral(L, "grid");
  lua_pushvalue(L, gridTableIndex);
  lua_rawset(L, palletCTableIndex);
}

void LuaInterface::setMonomeGridInterface(MonomeGridInterface& gridInterface) {
  this->gridInterface = &gridInterface;
  bindGrid(this->L);
}
}
