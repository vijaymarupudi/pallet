#pragma once
#include "Clock.hpp"
#include "lua.hpp"

class LuaInterface {
public:
  Clock* clock;
  lua_State* L;
private:
  void bind();
public:
  void init();
  void dostring(const char* str);
  void cleanup();
  void setClock(Clock* clock) {
    this->clock = clock;
  }

};

extern LuaInterface* luaInterface;

