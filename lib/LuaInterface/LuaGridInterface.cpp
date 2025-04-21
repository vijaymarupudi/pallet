#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"
#include "../luaHelper/LuaFunction.hpp"
#include "../luaHelper/LuaClass.hpp"

namespace pallet {

using namespace pallet::luaHelper;

  /*
 * Grid bindings
 */


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

namespace luaHelper {
template <>
struct LuaRetrieveContext<MonomeGridInterface&> {
  static inline MonomeGridInterface& retrieve(lua_State* L) {
    return *getLuaInterfaceObject(L).gridInterface;
  }
};
}

static void bindGrid(LuaInterface& iface, lua_State* L) {

  auto gridClassPointer = new LuaClass<GridWrapper>(L, "grid");
  iface.onDestroy.push_back([gridClassPointer]() {
    delete gridClassPointer;
  });

  auto& gridClass = *gridClassPointer;

  auto b = gridClass.beginBatch();

  gridClass.addStaticMethod("connect", [](MonomeGridInterface& iface, MonomeGridInterface::Id id) {
    return GridWrapper(&iface, id);
  });
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
  bindGrid(*this, this->L);
}
}
