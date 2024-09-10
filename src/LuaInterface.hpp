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
  void init(Clock* clock);
  void dostring(const char* str);
  void cleanup();
};

extern LuaInterface* luaInterface;

