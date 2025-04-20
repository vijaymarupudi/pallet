#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"
#include "../luaHelper/LuaFunction.hpp"
#include "../luaHelper/LuaClass.hpp"

namespace pallet {

using namespace pallet::luaHelper;

  /*
 * Grid bindings
 */

//   static int luaGridLed(lua_State* L) {
//     auto [grid, x, y, z] = checkedPullMultiple<
//       MonomeGrid*, int, int, int>(L, 1);
//     // auto grid = checkedPull<MonomeGrid*>(L, 1);
//     // int x = checkedPull<int>(L, 2);
//     // int y = checkedPull<int>(L, 3);
//     // int z = checkedPull<int>(L, 4);
//     grid->led(x - 1, y - 1, z);
//     return 0;
//   }

// static int luaGridAll(lua_State* L) {
//   auto grid = checkedPull<MonomeGrid*>(L, 1);
//   int z = checkedPull<int>(L, 2);
//   grid->all(z);
//   return 0;
// }

//   static int luaGridClear(lua_State* L) {
//     auto grid = checkedPull<MonomeGrid*>(L, 1);
//     grid->clear();
//     return 0;
//   }

//   static int luaGridRender(lua_State* L) {
//     auto grid = checkedPull<MonomeGrid*>(L, 1);
//     grid->render();
//     return 0;
//   }

  // static int luaGridSetOnKey(lua_State* L) {
  //   auto grid = checkedPull<MonomeGrid*>(L, 1);
  //   int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
  //   auto& iface = getLuaInterfaceObject(L);
  //   iface.gridKeyFunction = functionRef;
  //   grid->setOnKey([](int x, int y, int z, void* ud) {
  //     auto& luaInterface = *static_cast<LuaInterface*>(ud);
  //     lua_rawgeti(luaInterface.L, LUA_REGISTRYINDEX, luaInterface.gridKeyFunction);
  //     push(luaInterface.L, x + 1);
  //     push(luaInterface.L, y + 1);
  //     push(luaInterface.L, z);
  //     lua_call(luaInterface.L, 3, 0);
  //   }, &iface);
  //   return 0;
  // }


  // static int luaGridConnect(lua_State* L) {
  //   int id = checkedPull<int>(L, -2);
  //   // connect callback is the first argument
  //   int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
  //   auto& iface = getLuaInterfaceObject(L);
  //   iface.gridOnConnectFunction = functionRef;
  //   iface.gridInterface->setOnConnect([](const std::string& id, bool state,
  //                                        MonomeGrid* grid, void* ud) {
  //     (void)id;
  //     (void)state;
  //     if (!state) return;
  //     auto& luaInterface = *static_cast<LuaInterface*>(ud);
  //     lua_rawgeti(luaInterface.L, LUA_REGISTRYINDEX, luaInterface.gridOnConnectFunction);
  //     push(luaInterface.L, grid);
  //     lua_call(luaInterface.L, 1, 0);
  //   }, &iface);
  //   iface.gridInterface->connect(id);
  //   return 0;
  // }


class GridWrapper {
public:
  MonomeGridInterface* iface;
  MonomeGridInterface::Id id;
  GridWrapper(MonomeGridInterface* iface, MonomeGridInterface::Id id)
    : iface(iface), id(id) {}

  void led(int x, int y, int z) {
    return iface->led(id, x, y, z);
  }

  void all(int c) {
    return iface->all(id, c);
  }

  void clear() {
    return iface->clear(id);
  }

  void render() {
    return iface->render(id);
  }

  int getRows() const {
    return iface->getRows(id);
  }

  int getCols() const {
    return iface->getCols(id);
  }

  int getRotation() const {
    return iface->getRotation(id);
  }

  const char* getId() const {
    return iface->getId(id);
  }

  bool isConnected() const {
    return iface->isConnected(id);
  }

  MonomeGridInterface::KeyEventId listen(auto&& function) {
    return iface->listen(id, std::forward<decltype(function)>(function));
  }

  void unlisten(MonomeGridInterface::KeyEventId eid) {
    return iface->unlisten(id, eid);
  }
};

static void bindGrid(lua_State* L) {

  LuaClass<GridWrapper> gridClass(L, "grid");

  auto b = gridClass.beginBatch();
  
  gridClass.addMethodBatch<&GridWrapper::led>(b, "led");
  gridClass.addMethodBatch<&GridWrapper::all>(b, "all");
  gridClass.addMethodBatch<&GridWrapper::clear>(b, "clear");
  gridClass.addMethodBatch<&GridWrapper::render>(b, "render");
  gridClass.addMethodBatch<&GridWrapper::getRows>(b, "getRows");
  gridClass.addMethodBatch<&GridWrapper::getCols>(b, "getCols");
  gridClass.addMethodBatch<&GridWrapper::getRotation>(b, "getRotation");
  gridClass.addMethodBatch<&GridWrapper::getId>(b, "getId");
  gridClass.addMethodBatch<&GridWrapper::isConnected>(b, "isConnected");
  gridClass.addMethodBatch(b, "listen", [](GridWrapper* wrapper, LuaFunction<void(int, int, int)> func) {
    return wrapper->listen(std::move(func));
  });
  gridClass.addMethodBatch<&GridWrapper::unlisten>(b, "unlisten");
  gridClass.endBatch(b);

  getPalletCTable(L);
  auto palletTable = LuaTable{L, StackIndex{lua_gettop(L)}};
  palletTable.rawset("grid", gridClass);

  // int palletCTableIndex = lua_gettop(L);
  // lua_pushliteral(L, "grid");
  // lua_pushvalue(L, gridTableIndex);
  // lua_rawset(L, palletCTableIndex);
}

void LuaInterface::setMonomeGridInterface(MonomeGridInterface& gridInterface) {
  this->gridInterface = &gridInterface;
  bindGrid(this->L);
}
}
