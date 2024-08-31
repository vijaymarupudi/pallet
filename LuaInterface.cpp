#include "LuaInterface.hpp"
#include "filesystem.h"
#include <algorithm>
#include <utility>
#include <inttypes.h>
#include <string.h>

LuaInterface* luaInterface;

static void l_print_single(lua_State* L, int index) {
  int typ = lua_type(L, index);
  if (typ == LUA_TSTRING) {
    size_t len;
    const char* str = lua_tolstring(L, index, &len);
    fwrite(str, 1, len, stdout);
  } else if (typ == LUA_TNUMBER) {
    if (lua_isinteger(L, index)) {
      int64_t number = luaL_checknumber(L, index);
      printf("%lld", number);
    } else {
      float number = luaL_checknumber(L, index);
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

static int luaprint(lua_State* L) {
  int top = lua_gettop(L);
  for (int i = 1; i < top + 1; i++) {
    l_print_single(L, i);
    if (i != top) {
      putc(' ', stdout);
    }
  }
  putc('\n', stdout);
  return 0;
}

static void l_open_io(lua_State* L) {
  lua_register(L, "print", luaprint);
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

static void l_clock_set_timeout_cb(void* data);
static int l_clock_set_timeout(lua_State* L);
static int l_clock_set_interval(lua_State* L);
static int l_clock_current_time(lua_State* L);
static int l_clock_clear_timeout(lua_State* L);

static void l_bind_lua_func(lua_State* L, int table, const char* field, lua_CFunction f) {
  lua_pushcfunction(L, f);
  lua_setfield(L, table, field);
}

static void l_bind_clock(lua_State* L) {
  lua_newtable(L);
  lua_pushvalue(L, -1);
  lua_setfield(L, -3, "clock");
  // -1 is the clock table
  auto clock_table = lua_gettop(L);
  l_bind_lua_func(L, clock_table, "setTimeout", l_clock_set_timeout);
  l_bind_lua_func(L, clock_table, "setInterval", l_clock_set_interval);
  l_bind_lua_func(L, clock_table, "currentTime", l_clock_current_time);
  l_bind_lua_func(L, clock_table, "clearTimeout", l_clock_clear_timeout);
  l_bind_lua_func(L, clock_table, "clearInterval", l_clock_clear_timeout);
}

static const struct filesystem_entry* l_find_filesystem_entry(const struct filesystem_entry* entries, unsigned int entries_count, const char* fname) {
  auto low = entries;
  auto high = low + filesystem_lua_modules_count;
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
  auto state = (ReaderState*)ud;
  if (!state->state) {
    *size = state->size;
    state->state = true;
    return (const char*)state->data;
  } else {
    return nullptr;
  }
}

static int l_open_function(lua_State* L) {
  const char* lname = lua_tostring(L, -1);
  if (strcmp(lname, "pallet") == 0) {
    int top = lua_gettop(L);
    lua_newtable(L);
    l_bind_clock(L);
    lua_settop(L, top + 1);
    return 1;
  } else {
    const struct filesystem_entry* entry = l_find_filesystem_entry(&filesystem_lua_modules[0], filesystem_lua_modules_count,
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

void LuaInterface::bind() {
    auto L = this->L;
    lua_pushcfunction(L, l_require);
    lua_setglobal(L, "require");
}

void LuaInterface::init(Clock* clock) {
  this->L = luaL_newstate();
  l_open_libs(this->L);
  l_open_io(this->L);
  this->clock = clock;
  luaInterface = this;
  this->bind();
}

void LuaInterface::dostring(const char* str) {
    luaL_dostring(this->L, str);
  }


void LuaInterface::cleanup() {
  lua_close(this->L);
}


static void l_clock_set_timeout_cb(void* data) {
  int ref = reinterpret_cast<intptr_t>(data);
  lua_rawgeti(luaInterface->L, LUA_REGISTRYINDEX, ref);
  luaL_unref(luaInterface->L, LUA_REGISTRYINDEX, ref);
  // the function is now at the top of the stack
  lua_call(luaInterface->L, 0, 0);
}

static void l_clock_set_interval_cb(void* data) {
  int ref = reinterpret_cast<intptr_t>(data);
  lua_rawgeti(luaInterface->L, LUA_REGISTRYINDEX, ref);
  // the function is now at the top of the stack
  lua_call(luaInterface->L, 0, 0);
}


static int l_clock_set_timeout(lua_State* L) {
  uint64_t time = luaL_checkinteger(L, 1);
  int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
  auto id = luaInterface->clock->setTimeout(time, l_clock_set_timeout_cb, (void*)(intptr_t)functionRef);
  lua_pushinteger(L, id);
  return 1;
}
static int l_clock_set_interval(lua_State* L) {
  uint64_t time = luaL_checkinteger(L, 1);
  int functionRef = luaL_ref(L, LUA_REGISTRYINDEX);
  auto id = luaInterface->clock->setInterval(time, l_clock_set_interval_cb, (void*)(intptr_t)functionRef);
  lua_pushinteger(L, id);
  return 1;
}

static int l_clock_clear_timeout(lua_State* L) {
  int id = luaL_checkinteger(L, 1);
  int functionRef = (intptr_t)luaInterface->clock->getTimeoutUserData(id);
  luaL_unref(L, LUA_REGISTRYINDEX, functionRef);
  luaInterface->clock->clearTimeout(id);
  return 0;
}

static int l_clock_current_time(lua_State* L) {
  auto time = luaInterface->clock->currentTime();
  lua_pushinteger(L, time);
  return 1;
}
