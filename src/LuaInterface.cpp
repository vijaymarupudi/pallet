#include <algorithm>
#include <utility>
#include <inttypes.h>
#include <string.h>
#include <type_traits>

#include "LuaInterface.hpp"
#include "filesystem.h"
#include "luaHelper.hpp"

namespace pallet {

using namespace pallet::luaHelper;

static void luaPrintSingle(lua_State* L, int index) {
  int typ = lua_type(L, index);
  if (typ == LUA_TSTRING) {
    size_t len;
    const char* str = lua_tolstring(L, index, &len);
    fwrite(str, 1, len, stdout);
  } else if (typ == LUA_TNUMBER) {
    if (lua_isinteger(L, index)) {
      int64_t number = luaCheckedPull<int64_t>(L, index);
      printf("%ld", number);
    } else {
      float number = luaCheckedPull<float>(L, index);
      printf("%f", number);
    }

  } else if (typ == LUA_TNIL) {
    printf("nil");
  }
  else {
    const char* name;
    if (typ == LUA_TTABLE) {
      name = "table";
    } else if (typ == LUA_TFUNCTION) {
      name = "function";
    } else if (typ == LUA_TTHREAD) {
      name = "thread";
    } else if (typ == LUA_TUSERDATA) {
      name = "userdata";
    } else {
      name = "unknown";
    }
    printf("<%s %p>", name, lua_topointer(L, index));
  }
}

static int luaPrint(lua_State* L) {
  int top = lua_gettop(L);
  for (int i = 1; i < top + 1; i++) {
    luaPrintSingle(L, i);
    if (i != top) {
      putc(' ', stdout);
    }
  }
  putc('\n', stdout);
  return 0;
}

static void l_open_io(lua_State* L) {
  lua_register(L, "print", luaPrint);
}

static const luaL_Reg loadedlibs[] = {
  {"_G", luaopen_base},
  {LUA_COLIBNAME, luaopen_coroutine},
  {LUA_TABLIBNAME, luaopen_table},
  {LUA_STRLIBNAME, luaopen_string},
  {LUA_MATHLIBNAME, luaopen_math},
  {LUA_DBLIBNAME, luaopen_debug},
  {NULL, NULL}
};

static void l_open_libs(lua_State* L) {
  const luaL_Reg *lib;
  /* call open functions from 'loadedlibs' and set results to global table */
  for (lib = loadedlibs; lib->func; lib++) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);  /* remove lib */
  }
}

static const struct filesystem_entry* findFilesystemEntry(const struct filesystem_entry* entries, unsigned int entries_count, const char* fname) {
  auto low = entries;
  auto high = low + entries_count;
  while (low < high) {
    auto mid = low + ((high - low) / 2);
    auto cmp = strcmp(fname, mid->filename);
    if (cmp == 0) { return mid; }
    if (cmp < 0) {
      high = mid;
    } else {
      low = mid + 1;
    }
  }
  return nullptr;
}

struct ReaderState {
  const unsigned char* data;
  unsigned int size;
  bool state;
};

const char* l_lua_reader(lua_State* L, void* ud, size_t* size) {
  (void)L;
  auto state = (ReaderState*)ud;
  if (!state->state) {
    *size = state->size;
    state->state = true;
    return (const char*)state->data;
  } else {
    return nullptr;
  }
}

int getRegistryEntry(lua_State* L, const void* key) {
  lua_rawgetp(L, LUA_REGISTRYINDEX, key);
  return 1;
}

int getPalletCTable(lua_State* L) {
  return getRegistryEntry(L, &__palletCTableRegistryIndex);
}

LuaInterface& getLuaInterfaceObject(lua_State* L) {
  getRegistryEntry(L, &__luaInterfaceRegistryIndex);
  auto ret = luaPull<LuaInterface*>(L, -1);
  lua_pop(L, 1);
  return *ret;
}

static int l_open_function(lua_State* L) {
  const char* lname = lua_tostring(L, -1);
  if (strcmp(lname, "__pallet") == 0) {
    return getPalletCTable(L);
  } else {
    const struct filesystem_entry* entry =
      findFilesystemEntry(&filesystem_lua_modules[0],
                              filesystem_lua_modules_count,
                              lname);

    if (!entry) {
      lua_pushnil(L);
    } else {
      ReaderState state { entry->contents, entry->size, false };
      lua_load(L, l_lua_reader, &state, lname, NULL);
      lua_call(L, 0, 1);
    }
    return 1;
  }
}

static int l_require(lua_State* L) {
  size_t len;
  const char* str = lua_tolstring(L, -1, &len);
  luaL_requiref(L, str, l_open_function, 0);
  return 1;
}

void LuaInterface::setupRequire() {
  auto L = this->L;
  luaPush(L, l_require);
  lua_setglobal(L, "require");
}

LuaInterface::LuaInterface() {
  this->L = luaL_newstate();
  l_open_libs(this->L);
  l_open_io(this->L);

  // Setting up lightuserdata for the LuaInterface object
  luaPush(this->L, this);
  lua_rawsetp(this->L, LUA_REGISTRYINDEX, &__luaInterfaceRegistryIndex);

  // Setting up __pallet table
  lua_newtable(this->L);
  lua_rawsetp(this->L, LUA_REGISTRYINDEX, &__palletCTableRegistryIndex);

  this->setupRequire();
}

int LuaInterface::dostring(const char* str) {
  return luaL_dostring(this->L, str);
}


LuaInterface::~LuaInterface() {
  lua_close(this->L);
}


/*
 * Grid bindings
 */

  static int luaGridLed(lua_State* L) {
    auto grid = luaCheckedPull<MonomeGrid*>(L, 1);
    int x = luaCheckedPull<int>(L, 2);
    int y = luaCheckedPull<int>(L, 3);
    int z = luaCheckedPull<int>(L, 4);
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
      auto thi = (LuaInterface*)ud;
      lua_rawgeti(thi->L, LUA_REGISTRYINDEX, thi->gridKeyFunction);
      luaPush(thi->L, x + 1);
      luaPush(thi->L, y + 1);
      luaPush(thi->L, z);
      lua_call(thi->L, 3, 0);
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
      auto thi = (LuaInterface*)ud;
      lua_rawgeti(thi->L, LUA_REGISTRYINDEX, thi->gridOnConnectFunction);
      luaPush(thi->L, grid);
      lua_call(thi->L, 1, 0);
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

  /*
   *
   * Clock bindings
   *
   */

static void luaClockSetTimeoutCb(ClockEventInfo* info, void* data);
static void luaClockSetIntervalCb(ClockEventInfo* info, void* data);
static int luaClockSetTimeout(lua_State* L);
static int luaClockSetInterval(lua_State* L);
static int luaClockCurrentTime(lua_State* L);
static int luaClockClearTimeout(lua_State* L);

static void bindClock(lua_State* L) {
  auto start = lua_gettop(L);
  lua_newtable(L); // clock
  int clockTableIndex = lua_gettop(L);
  luaRawSetTable(L, clockTableIndex, "setTimeout", luaClockSetTimeout);
  luaRawSetTable(L, clockTableIndex, "setInterval", luaClockSetInterval);
  luaRawSetTable(L, clockTableIndex, "currentTime", luaClockCurrentTime);
  luaRawSetTable(L, clockTableIndex, "clearTimeout", luaClockClearTimeout);
  luaRawSetTable(L, clockTableIndex, "clearInterval", luaClockClearTimeout);
  getPalletCTable(L);
  auto palletCTableIndex = lua_gettop(L);
  lua_pushliteral(L, "clock");
  lua_pushvalue(L, clockTableIndex);
  lua_rawset(L, palletCTableIndex);
  lua_settop(L, start);
}

void LuaInterface::setClock(Clock& clock) {
  this->clock = &clock;
  bindClock(this->L);
}

static uint64_t luaClockGetTimeArgument(lua_State* L, int index) {
  if (lua_isinteger(L, index)) {
    return lua_tointeger(L, index);
  } else {
    return lua_tonumber(L, index);
  }
}

static int luaClockSetTimeout(lua_State* L) {
  uint64_t time = luaClockGetTimeArgument(L, 1);
  // the user's function is at the top of the stack, so store it at the index
  int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
  auto& luaInterface = getLuaInterfaceObject(L);
  auto id = luaInterface.clockCallbackState.push(LuaInterface::ClockCallbackStateEntry {
      luaInterface, 0, 0, functionRef
    });
  auto& state = luaInterface.clockCallbackState[id];
  auto cid = luaInterface.clock->setTimeout(time, luaClockSetTimeoutCb, &state);
  state.id = id;
  state.clockId = cid;
  state.luaFunctionRef = functionRef;
  luaPush(L, id);
  return 1;
}

static int luaClockSetInterval(lua_State* L) {
  uint64_t time = luaClockGetTimeArgument(L, 1);
  // the user's function is at the top of the stack
  int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
  auto& luaInterface = getLuaInterfaceObject(L);
  auto id = luaInterface.clockCallbackState.push(LuaInterface::ClockCallbackStateEntry {luaInterface, 0, 0, functionRef});
  auto& state = luaInterface.clockCallbackState[id];
  auto cid = luaInterface.clock->setInterval(time, luaClockSetIntervalCb, &state);
  state.id = id;
  state.clockId = cid;
  state.luaFunctionRef = functionRef;
  luaPush(L, id);
  return 1;
}

static void luaClockStateCleanup(LuaInterface::ClockCallbackStateEntry& entry, bool cancel = false) {
  auto& luaInterface = entry.luaInterface;
  auto L = luaInterface.L;
  auto id = entry.id;
  auto clockId = entry.clockId;
  auto ref = entry.luaFunctionRef;

  if (cancel) {
    luaInterface.clock->clearTimeout(clockId);
  }

  luaL_unref(L, LUA_REGISTRYINDEX, ref);
  luaInterface.clockCallbackState.free(id);
}

static void luaClockSetTimeoutCb(ClockEventInfo* info, void* data) {
  (void)info;
  auto& state = *static_cast<LuaInterface::ClockCallbackStateEntry*>(data);
  auto& luaInterface = state.luaInterface;
  auto ref = state.luaFunctionRef;
  lua_rawgeti(luaInterface.L, LUA_REGISTRYINDEX, ref);
  lua_call(luaInterface.L, 0, 0);
  luaClockStateCleanup(state);
}

static void luaClockSetIntervalCb(ClockEventInfo* info, void* data) {
  (void)info;
  auto& state = *static_cast<LuaInterface::ClockCallbackStateEntry*>(data);
  auto& luaInterface = state.luaInterface;
  auto ref = state.luaFunctionRef;
  lua_rawgeti(luaInterface.L, LUA_REGISTRYINDEX, ref);
  lua_call(luaInterface.L, 0, 0);
}

static int luaClockClearTimeout(lua_State* L) {
  int id = luaCheckedPull<int>(L, 1);
  auto& luaInterface = getLuaInterfaceObject(L);
  auto& state = luaInterface.clockCallbackState[id];
  luaClockStateCleanup(state, true);
  return 0;
}

static int luaClockCurrentTime(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  auto time = luaInterface.clock->currentTime();
  luaPush(L, time);
  return 1;
}

// static int luaBeatClockSyncTimeout(lua_State* L) {
//   auto& luaInterface = getLuaInterfaceObject(L);
//   auto beatClock = luaInterface.beatClock;
//   (void)beatClock;
//   return 1;
// }

static void bindBeatClock(lua_State* L) {
  (void)L;
  // auto start = lua_gettop(L);
  // lua_newtable(L); // BeatClock
  // int beatClockTableIndex = lua_gettop(L);
  // luaRawSetTable(L, beatClockTableIndex, "setBeatSyncTimeout", luaClockClearTimeout);

}

void LuaInterface::setBeatClock(BeatClock& beatClock) {
  this->beatClock = &beatClock;
  bindBeatClock(this->L);
}
}
