#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"


namespace pallet {

using namespace pallet::luaHelper;

static int luaScreenRender(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  luaInterface.graphicsInterface->render();
  return 0;
}

static int luaScreenQuit(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  luaInterface.graphicsInterface->quit();
  return 0;
}

static int luaScreenClear(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  luaInterface.graphicsInterface->clear();
  return 0;
}

static int luaScreenRect(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  auto [x, y, w, h, c] =
    checkedPullMultiple<float, float, float, float, int>(L, 1);

  luaInterface.graphicsInterface->rect(x - 1, y - 1, w, h, c);
  return 0;
}

static int luaScreenStrokeRect(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  auto [x, y, w, h, c] =
    checkedPullMultiple<float, float, float, float, int>(L, 1);

  luaInterface.graphicsInterface->strokeRect(x - 1, y - 1, w, h, c);
  return 0;
}

static int luaScreenPoint(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  auto [x, y, c] =
    checkedPullMultiple<float, float, int>(L, 1);
  luaInterface.graphicsInterface->point(x - 1, y - 1, c);
  return 0;
}

static int luaScreenText(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  auto [x, y, str, fc, bc, align, baseline] =
    checkedPullMultiple<float, float, std::string_view,
                           int, int, pallet::GraphicsPosition,
                           pallet::GraphicsPosition>(L, 1);
  luaInterface.graphicsInterface->text(x - 1, y - 1, str, fc, bc, align, baseline);
  return 0;
}

static int luaGraphicsEventToTable(lua_State* L,
                                   const pallet::GraphicsEvent& event) {
  auto& luaInterface = getLuaInterfaceObject(L);
  lua_newtable(L);
  auto tableIndex = lua_gettop(L);
  lua_pushliteral(L, "type");
  int ref = luaInterface.graphicsEventStringsRef[event.index()];
  lua_rawgeti(L, LUA_REGISTRYINDEX, ref);
  lua_rawset(L, tableIndex);

  auto visitor = overloads
    {
      [&](const pallet::GraphicsEventMouseButton& e) {
        rawSetTable(L, tableIndex, "x", e.x + 1);
        rawSetTable(L, tableIndex, "y", e.y + 1);
        rawSetTable(L, tableIndex, "state", e.state);
        rawSetTable(L, tableIndex, "button", e.button);
      },
      [&](const pallet::GraphicsEventMouseMove& e) {
        rawSetTable(L, tableIndex, "x", e.x + 1);
        rawSetTable(L, tableIndex, "y", e.y + 1);
      },
      [&](const pallet::GraphicsEventKey& e) {
        rawSetTable(L, tableIndex, "repeat", e.repeat);
        rawSetTable(L, tableIndex, "state", e.state);
        rawSetTable(L, tableIndex, "keycode", e.keycode);
        rawSetTable(L, tableIndex, "scancode", e.scancode);
        rawSetTable(L, tableIndex, "mod", e.mod);
      },
      [&](const pallet::GraphicsEventQuit&) {
      
      }
    };
  pallet::visit(std::move(visitor), event);
  return 1;
}

static int luaScreenSetOnEvent(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  if (lua_isnil(L, -1)) {
    if (!(luaInterface.screenOnEventRef < 0)) {
      luaL_unref(L, LUA_REGISTRYINDEX, luaInterface.screenOnEventRef);
      luaInterface.screenOnEventRef = -1;
    }
  } else {
    if (!(luaInterface.screenOnEventRef < 0)) {
      luaL_unref(L, LUA_REGISTRYINDEX, luaInterface.screenOnEventRef);
      luaInterface.screenOnEventRef = -1;
    }
    luaInterface.screenOnEventRef = luaL_ref(L, LUA_REGISTRYINDEX);
  }
  return 0;
}


static void bindGraphicsInterface(lua_State* L) {
  auto start = lua_gettop(L);
  lua_newtable(L); // screen
  int screenTableIndex = lua_gettop(L);

  auto& luaInterface = getLuaInterfaceObject(L);

  rawSetTable(L, screenTableIndex, "render", luaScreenRender);
  rawSetTable(L, screenTableIndex, "quit", luaScreenQuit);
  rawSetTable(L, screenTableIndex, "clear", luaScreenClear);
  rawSetTable(L, screenTableIndex, "rect", luaScreenRect);
  rawSetTable(L, screenTableIndex, "strokeRect", luaScreenStrokeRect);
  rawSetTable(L, screenTableIndex, "point", luaScreenPoint);
  rawSetTable(L, screenTableIndex, "text", luaScreenText);
  rawSetTable(L, screenTableIndex, "setOnEvent", luaScreenSetOnEvent);

  // enums
  rawSetTable(L, screenTableIndex, "Default",
                 static_cast<int>(pallet::GraphicsPosition::Default));
  rawSetTable(L, screenTableIndex, "Center",
                 static_cast<int>(pallet::GraphicsPosition::Center));
  rawSetTable(L, screenTableIndex, "Bottom",
                 static_cast<int>(pallet::GraphicsPosition::Bottom));

  // intern and store graphics event strings

  lightVariantForEach<pallet::GraphicsEvent>([&]<class EventType, size_t i>() {
      auto str = getGraphicsEventName<EventType>();
      lua_pushstring(L, str);
      int ref = luaL_ref(L, LUA_REGISTRYINDEX);
      luaInterface.graphicsEventStringsRef[i] = ref;
      // rawSetTable(L, screenTableIndex, pallet::getGraphicsEventName<EventType>(), i);
    });


  getPalletCTable(L);
  auto palletCTableIndex = lua_gettop(L);
  lua_pushliteral(L, "screen");
  lua_pushvalue(L, screenTableIndex);
  lua_rawset(L, palletCTableIndex);
  lua_settop(L, start);
}

void LuaInterface::setGraphicsInterface(GraphicsInterface& graphicsInterface) {
  this->graphicsInterface = &graphicsInterface;
  bindGraphicsInterface(this->L);
  graphicsInterface.setOnEvent([](pallet::GraphicsEvent e, void* ud) {
    auto L = static_cast<lua_State*>(ud);
    auto& luaInterface = getLuaInterfaceObject(L);
    if (!(luaInterface.screenOnEventRef < 0)) {
      getRegistryEntry(L, luaInterface.screenOnEventRef);
      auto nargs = luaGraphicsEventToTable(L, e);
      lua_call(L, nargs, 0);
    }
  }, L);
}

}
