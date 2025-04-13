#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"

namespace pallet {

using namespace pallet::luaHelper;

static void luaClockSetTimeoutIntervalCb(const ClockInterface::EventInfo& info, void* data);
static int luaClockSetTimeout(lua_State* L);
static int luaClockSetInterval(lua_State* L);
static int luaClockCurrentTime(lua_State* L);
static int luaClockClearTimeout(lua_State* L);
static int luaClockTimeInMs(lua_State* L);

static void bindClock(lua_State* L) {
  auto start = lua_gettop(L);
  getPalletCTable(L);
  auto palletCTableIndex = lua_gettop(L);
  lua_newtable(L); // clock
  int clockTableIndex = lua_gettop(L);
  luaRawSetTable(L, clockTableIndex, "setTimeout", luaClockSetTimeout);
  luaRawSetTable(L, clockTableIndex, "setInterval", luaClockSetInterval);
  luaRawSetTable(L, clockTableIndex, "currentTime", luaClockCurrentTime);
  luaRawSetTable(L, clockTableIndex, "clearTimeout", luaClockClearTimeout);
  luaRawSetTable(L, clockTableIndex, "clearInterval", luaClockClearTimeout);
  luaRawSetTable(L, clockTableIndex, "timeInMs", luaClockTimeInMs);
  lua_pushliteral(L, "clock");
  lua_pushvalue(L, clockTableIndex);
  lua_rawset(L, palletCTableIndex);
  lua_settop(L, start);
}

void LuaInterface::setClockInterface(ClockInterface& clockInterface) {
  this->clockInterface = &clockInterface;
  bindClock(this->L);
}

static uint64_t luaClockGetTimeArgument(lua_State* L, int index) {
  if (lua_isinteger(L, index)) {
    return lua_tointeger(L, index);
  } else {
    return lua_tonumber(L, index);
  }
}

static int luaClockTimeInMs(lua_State* L) {
  auto time = luaClockGetTimeArgument(L, 1);
  luaPush(L, pallet::timeInMs(time));
  return 1;
}

static int _luaClockSetTimeout(lua_State* L, bool interval) {
  uint64_t time = luaClockGetTimeArgument(L, 1);
  // the user's function is at the top of the stack, so store it at the index
  int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
  auto& luaInterface = getLuaInterfaceObject(L);
  auto id = luaInterface.clockCallbackState.push(LuaInterface::ClockCallbackStateEntry {
      luaInterface, 0, 0, false, functionRef
    });
  auto& state = luaInterface.clockCallbackState[id];
  Clock::Id cid;

  if (interval) {
    cid = luaInterface.clockInterface->setInterval(time, {luaClockSetTimeoutIntervalCb, &state});
  } else {
    cid = luaInterface.clockInterface->setTimeout(time, {luaClockSetTimeoutIntervalCb, &state});
  }
  state.interval = interval;
  state.id = id;
  state.clockId = cid;
  state.luaFunctionRef = functionRef;
  luaPush(L, id);
  return 1;
}

static int luaClockSetTimeout(lua_State* L) {
  return _luaClockSetTimeout(L, false);
}

static int luaClockSetInterval(lua_State* L) {
  return _luaClockSetTimeout(L, true);
}

static void luaClockStateCleanup(LuaInterface::ClockCallbackStateEntry& entry, bool cancel = false) {
  auto& luaInterface = entry.luaInterface;
  auto L = luaInterface.L;
  auto id = entry.id;
  auto clockId = entry.clockId;
  auto ref = entry.luaFunctionRef;

  if (cancel) {
    luaInterface.clockInterface->clearTimeout(clockId);
  }

  luaL_unref(L, LUA_REGISTRYINDEX, ref);
  luaInterface.clockCallbackState.free(id);
}

static void luaClockSetTimeoutIntervalCb(const ClockInterface::EventInfo& info, void* data) {
  (void)info;
  auto& state = *static_cast<LuaInterface::ClockCallbackStateEntry*>(data);
  auto& luaInterface = state.luaInterface;
  auto ref = state.luaFunctionRef;
  lua_rawgeti(luaInterface.L, LUA_REGISTRYINDEX, ref);
  lua_call(luaInterface.L, 0, 0);
  if (!state.interval) {
    luaClockStateCleanup(state);
  }
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
  auto time = luaInterface.clockInterface->currentTime();
  luaPush(L, time);
  return 1;
}

}
