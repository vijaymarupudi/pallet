#include <algorithm>
#include <utility>
#include <cstdint>
#include <string.h>
#include <type_traits>

#include "variant.hpp"
#include "LuaInterface.hpp"
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

Result<LuaInterface> LuaInterface::create() {
  return Result<LuaInterface>(std::in_place_t{});
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
  int ret = luaL_dostring(this->L, str);
  if (ret) {
    size_t len;
    const char* s = lua_tolstring(this->L, -1, &len);
    printf("error: %s\n", s);
  }
  return ret;
}


LuaInterface::~LuaInterface() {
  lua_close(this->L);
}

}
