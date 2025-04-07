#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"

namespace pallet {

using namespace pallet::luaHelper;

static void luaBeatClockStateCleanup(LuaInterface::BeatClockCallbackStateEntry& entry, bool cancel = false) {
  auto& luaInterface = entry.luaInterface;
  auto L = luaInterface.L;
  auto id = entry.id;
  auto beatClockId = entry.beatClockId;
  auto ref = entry.luaFunctionRef;
  if (cancel) {
    luaInterface.beatClock->clearBeatSyncTimeout(beatClockId);
  }

  luaL_unref(L, LUA_REGISTRYINDEX, ref);
  luaInterface.beatClockCallbackState.free(id);
}

static void luaBeatClockSetTimeoutIntervalCb(const BeatClockEventInfo& info,
                                             void* data) {
  (void)info;
  auto& state = *static_cast<LuaInterface::BeatClockCallbackStateEntry*>(data);
  auto& luaInterface = state.luaInterface;
  auto ref = state.luaFunctionRef;
  lua_rawgeti(luaInterface.L, LUA_REGISTRYINDEX, ref);
  lua_call(luaInterface.L, 0, 0);
  if (!state.interval) {
    luaBeatClockStateCleanup(state);
  }
}

static int _luaBeatClockSyncTimeout(lua_State* L, bool interval) {
  double sync = 0, offset = 0, period = 0;
  if (interval) {
    auto [_sync, _offset, _period] = luaCheckedPullMultiple<double, double, double>(L, 1);
    sync = _sync;
    offset = _offset;
    period = _period;
  } else {
    auto [_sync, _offset] = luaCheckedPullMultiple<double, double>(L, 1);
    sync = _sync;
    offset = _offset;
  }

  int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
  auto& luaInterface = getLuaInterfaceObject(L);
  auto& beatClock = *luaInterface.beatClock;
  auto id = luaInterface.beatClockCallbackState.push(LuaInterface::BeatClockCallbackStateEntry {
      luaInterface, 0, 0, false, functionRef
    });
  auto& state = luaInterface.beatClockCallbackState[id];
  BeatClock::Id cid;
  if (interval) {
    cid = beatClock.setBeatSyncInterval(sync, offset, period,
                                        {luaBeatClockSetTimeoutIntervalCb,
                                         &state});
  }
  else {
    cid = beatClock.setBeatSyncTimeout(sync, offset, {luaBeatClockSetTimeoutIntervalCb, &state});
  }
  state.interval = interval;
  state.id = id;
  state.beatClockId = cid;
  state.luaFunctionRef = functionRef;
  luaPush(L, id);
  return 1;
}

static int luaBeatClockSyncTimeout(lua_State* L) {
  return _luaBeatClockSyncTimeout(L, false);
}

static int luaBeatClockSyncInterval(lua_State* L) {
  return _luaBeatClockSyncTimeout(L, true);
}

static int luaBeatClockClearBeatSyncTimeout(lua_State* L) {
  int id = luaCheckedPull<int>(L, 1);
  auto& luaInterface = getLuaInterfaceObject(L);
  auto& state = luaInterface.beatClockCallbackState[id];
  luaBeatClockStateCleanup(state, true);
  return 0;
}

static int luaBeatClockSetBPM(lua_State* L) {
  auto bpm = luaCheckedPull<double>(L, 1);
  auto& luaInterface = getLuaInterfaceObject(L);
  luaInterface.beatClock->setBPM(bpm);
  return 0;
}

static int luaBeatClockCurrentBeat(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  auto b = luaInterface.beatClock->currentBeat();
  luaPush(L, b);
  return 1;
}

static void bindBeatClock(lua_State* L) {
  auto start = lua_gettop(L);
  lua_newtable(L); // BeatClock
  int beatClockTableIndex = lua_gettop(L);
  luaRawSetTable(L, beatClockTableIndex, "setBeatSyncTimeout", luaBeatClockSyncTimeout);
  luaRawSetTable(L, beatClockTableIndex, "setBeatSyncInterval", luaBeatClockSyncInterval);
  luaRawSetTable(L, beatClockTableIndex, "clearBeatSyncTimeout", luaBeatClockClearBeatSyncTimeout);
  luaRawSetTable(L, beatClockTableIndex, "clearBeatSyncInterval", luaBeatClockClearBeatSyncTimeout);
  luaRawSetTable(L, beatClockTableIndex, "setBPM", luaBeatClockSetBPM);
  luaRawSetTable(L, beatClockTableIndex, "currentBeat", luaBeatClockCurrentBeat);
  getPalletCTable(L);
  auto palletCTableIndex = lua_gettop(L);
  lua_pushliteral(L, "beatClock");
  lua_pushvalue(L, beatClockTableIndex);
  lua_rawset(L, palletCTableIndex);
  lua_settop(L, start);
}

void LuaInterface::setBeatClock(BeatClock& beatClock) {
  this->beatClock = &beatClock;
  bindBeatClock(this->L);
}

}
