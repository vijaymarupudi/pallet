#include "pallet/LuaInterface.hpp"
#include "../LuaInterfaceImpl.hpp"

namespace pallet {

using namespace pallet::luaHelper;

static int luaSendMidi(lua_State* L) {
  auto& luaInterface = getLuaInterfaceObject(L);
  if (lua_istable(L, -1)) {
    auto tableIndex = lua_gettop(L);
    lua_len(L, tableIndex);
    size_t len = lua_tointeger(L, -1);
    unsigned char buf[2048];
    for (size_t i = 0; i < len; i++) {
      auto type = lua_rawgeti(L, tableIndex, i + 1);
      unsigned char val = 0;
      if (type == LUA_TNUMBER) {
        auto num = lua_tointeger(L, -1);
        if (num < 256) {
          val = num;
        }
      }
      lua_pop(L, 1);
      // printf("%lld %lld\n", i, val);
      buf[i] = val;
    }
    luaInterface.midiInterface->sendMidi(buf, len);
  }
  return 0;
}

static void bindMidiInterface(lua_State* L) {
  auto start = lua_gettop(L);
  lua_newtable(L); // midi
  int midiTableIndex = lua_gettop(L);
  
  luaRawSetTable(L, midiTableIndex, "sendMidi", luaSendMidi);

  getPalletCTable(L);
  auto palletCTableIndex = lua_gettop(L);
  lua_pushliteral(L, "midi");
  lua_pushvalue(L, midiTableIndex);
  lua_rawset(L, palletCTableIndex);
  lua_settop(L, start);
}

void LuaInterface::setMidiInterface(MidiInterface& midiInterface) {
  this->midiInterface = &midiInterface;
  bindMidiInterface(this->L);
}

}
