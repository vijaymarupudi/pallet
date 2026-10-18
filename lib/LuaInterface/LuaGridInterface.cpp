#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"
#include "../luaHelper/LuaFunction.hpp"
#include "../luaHelper/LuaClass.hpp"
#include "../luaHelper/variant.hpp"

namespace pallet {

using namespace pallet::luaHelper;

char GridWrapperKey = 'g';

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


using GridLuaClass = LuaClass<GridWrapper>;

namespace luaHelper {

template <>
struct LuaTraits<GridWrapper> : public GridLuaClass::LuaTraits {};

template <>
struct LuaRetrieveContext<MonomeGridInterface&> {
  static inline MonomeGridInterface& retrieve(lua_State* L) {
    return *getLuaInterfaceObject(L).gridInterface;
  }
};
}

// template <>
// struct LuaTraits<GridWrapper> {
//   static inline void push(lua_State* L, concepts::DecaysTo<GridWrapper> auto&& wrapper) {

//   }
// };


static void bindGrid(lua_State* L) {

  using pallet::luaHelper::cw;

  auto& gridClass = GridLuaClass::create(L, "grid");

  auto b = gridClass.beginBatch();

  // grid.connect(index, func) or grid.connect(serial, func). The serial
  // is stable across runs; the index is the discovery order.
  gridClass.addStaticMethod("connect", [](MonomeGridInterface& iface,
                                          Variant<int, std::string_view> which,
                                          LuaFunction func) {
    auto onConnect = [func = std::move(func),
                      &iface] (auto&& result)
                     mutable {
      if (result) {
        func.call(GridWrapper(&iface, *result));
      } else {
        func.call(LuaNil);
      }
    };
    if (auto* luaId = pallet::get_if<int>(which)) {
      // Lua indexes grids from 1, like the screen coordinates
      if (*luaId < 1) {
        onConnect(Result<MonomeGridInterface::Id>(pallet::error(
          std::make_error_condition(std::errc::invalid_argument))));
        return;
      }
      iface.connect(static_cast<MonomeGridInterface::Id>(*luaId - 1), std::move(onConnect));
    } else {
      // the string_view points into the Lua stack; connect() copies it
      iface.connect(pallet::get_unchecked<std::string_view>(which), std::move(onConnect));
    }
  });

  gridClass.addMethodBatch(b, "led", cw<&GridWrapper::led>);
  gridClass.addMethodBatch(b, "all", cw<&GridWrapper::all>);
  gridClass.addMethodBatch(b, "clear", cw<&GridWrapper::clear>);
  gridClass.addMethodBatch(b, "render", cw<&GridWrapper::render>);
  gridClass.addMethodBatch(b, "getRows", cw<&GridWrapper::getRows>);
  gridClass.addMethodBatch(b, "getCols", cw<&GridWrapper::getCols>);
  gridClass.addMethodBatch(b, "getRotation", cw<&GridWrapper::getRotation>);
  gridClass.addMethodBatch(b, "getId", cw<&GridWrapper::getId>);
  gridClass.addMethodBatch(b, "isConnected", cw<&GridWrapper::isConnected>);
  gridClass.addMethodBatch(b, "listen", [](GridWrapper* wrapper,
                                           LuaFunction func) {
    return wrapper->listen([func=std::move(func)](int x, int y, int z) mutable {
      func.call(x, y, z);
    });
  });
  gridClass.addMethodBatch(b, "unlisten", cw<&GridWrapper::unlisten>);
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
