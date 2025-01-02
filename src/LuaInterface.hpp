#pragma once
#include "Clock.hpp"
#include "lua.hpp"

namespace pallet {

class LuaInterface {
public:
  Clock* clock;
  lua_State* L;
private:
  void bind();
public:
  LuaInterface();
  ~LuaInterface();
  void dostring(const char* str);
  void cleanup();
  void setClock(Clock* clock) {
    this->clock = clock;
  }
};

extern LuaInterface* luaInterface;

}



