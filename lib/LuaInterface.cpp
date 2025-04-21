#include <algorithm>
#include <utility>
#include <cstdint>
#include <string.h>
#include <type_traits>

#include "pallet/LuaInterface.hpp"

#include "filesystem.h"
#include "LuaInterfaceImpl.hpp"

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
      int64_t number = checkedPull<int64_t>(L, index);
      printf("%ld", number);
    } else {
      float number = checkedPull<float>(L, index);
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

static void luaOpenIo(lua_State* L) {
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

static void luaOpenLibs(lua_State* L) {
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
  const char* data;
  unsigned int size;
  bool state;
};

const char* luaLuaReader(lua_State* L, void* ud, size_t* size) {
  (void)L;
  auto state = static_cast<ReaderState*>(ud);
  if (!state->state) {
    *size = state->size;
    state->state = true;
    return static_cast<const char*>(state->data);
  } else {
    return nullptr;
  }
}

static int luaOpenFunction(lua_State* L) {
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
      ReaderState state { reinterpret_cast<const char*>(entry->contents), entry->size, false };
      lua_load(L, luaLuaReader, &state, lname, NULL);
      lua_call(L, 0, 1);
    }
    return 1;
  }
}

static int luaRequire(lua_State* L) {
  size_t len;
  const char* str = lua_tolstring(L, -1, &len);
  luaL_requiref(L, str, luaOpenFunction, 0);
  return 1;
}

void LuaInterface::setupRequire() {
  auto L = this->L;
  push(L, luaRequire);
  lua_setglobal(L, "require");
}

Result<LuaInterface> LuaInterface::create() {
  return Result<LuaInterface>(std::in_place_t{});
}

extern const int _luaInterfaceRegistryIndex = 0;
extern const int _palletCTableRegistryIndex = 0;

LuaInterface::LuaInterface() {
  this->L = luaL_newstate();
  luaOpenLibs(this->L);
  luaOpenIo(this->L);

  // Setting up lightuserdata for the LuaInterface object
  push(this->L, this);
  lua_rawsetp(this->L, LUA_REGISTRYINDEX, &_luaInterfaceRegistryIndex);

  // Setting up __pallet table
  lua_newtable(this->L);
  lua_rawsetp(this->L, LUA_REGISTRYINDEX, &_palletCTableRegistryIndex);
  this->setupRequire();
}

int LuaInterface::dostring(const char* str) {
  int ret = luaL_dostring(this->L, str);
  if (ret) {
    size_t len;
    const char* s = lua_tolstring(this->L, -1, &len);
    printf("error: %s\n", s);
  }
  return ret;
}


LuaInterface::~LuaInterface() {
  for (auto&& item : this->onDestroy) {
    std::move(item)();
  }
  lua_close(this->L);
}


static void luaBindPlatform(lua_State* L) {
  getPalletCTable(L);
  auto palletCTableIndex = lua_gettop(L);
  rawSetTable(L, palletCTableIndex, "quit",
                 +[](lua_State* L) {
                   auto& luaInterface = getLuaInterfaceObject(L);
                   luaInterface.platform->quit();
                   return 0;
                 });
}

void LuaInterface::setPlatform(Platform& platform) {
  this->platform = &platform;
  luaBindPlatform(this->L);
}

}
