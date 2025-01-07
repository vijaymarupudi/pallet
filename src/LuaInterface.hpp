#pragma once
#include <type_traits>
#include <deque>

#include "lua.hpp"

#include "IdTable.hpp"
#include "Clock.hpp"
#include "MonomeGridInterface.hpp"
#include "StaticVector.hpp"

namespace pallet {

// Only the addresses of these values are used
const int __luaInterfaceRegistryIndex = 0;
const int __palletCTableRegistryIndex = 0;

class LuaInterface {
public:

  lua_State* L;

  /*
   * Clock binding state
   */

  using id_type = uint32_t;
  struct ClockCallbackStateEntry {
    LuaInterface& luaInterface;
    id_type id;
    Clock::id_type clockId;
    int luaFunctionRef;
  };

  // I need pointer stability, cannot use vector
  template <class T>
  using IdTableContainer = std::conditional_t<pallet::constants::isEmbeddedDevice,
                                              containers::StaticVector<T, 256>,
                                              std::deque<T>>;
  Clock* clock = nullptr;
  containers::IdTable<ClockCallbackStateEntry, IdTableContainer, id_type> clockCallbackState;

  /*
   * Grid binding state
   */

  MonomeGridInterface* gridInterface;
  int gridOnConnectFunction = 0;
  int gridKeyFunction = 0;

private:
  void setupRequire();
public:
  LuaInterface();
  ~LuaInterface();
  int dostring(const char* str);
  void setClock(Clock& clock);
  void setMonomeGridInterface(MonomeGridInterface& gridInterface);
};

}

