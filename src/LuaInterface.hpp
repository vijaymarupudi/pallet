#pragma once
#include <type_traits>
#include <deque>
#include <array>

#include "lua.hpp"

#include "containers/IdTable.hpp"
#include "containers/StaticVector.hpp"
#include "Clock.hpp"
#include "MonomeGridInterface.hpp"
#include "BeatClock.hpp"
#include "GraphicsInterface.hpp"
#include "MidiInterface.hpp"

namespace pallet {

// Only the addresses of these values are used
const int __luaInterfaceRegistryIndex = 0;
const int __palletCTableRegistryIndex = 0;

class LuaInterface {
public:

  using id_type = uint32_t;

  // I need pointer stability, cannot use std::vector
  template <class T>
  using IdTableContainer = std::conditional_t<pallet::constants::isEmbeddedDevice,
                                              containers::StaticVector<T, 256>,
                                              std::deque<T>>;

  lua_State* L;

  /*
   * Clock binding state
   */

  struct ClockCallbackStateEntry {
    LuaInterface& luaInterface;
    id_type id;
    Clock::id_type clockId;
    bool interval;
    int luaFunctionRef = 0;
  };

  Clock* clock = nullptr;
  containers::IdTable<ClockCallbackStateEntry,
                      IdTableContainer,
                      id_type> clockCallbackState;

  /*
   * BeatClock binding state
   */

  struct BeatClockCallbackStateEntry {
    LuaInterface& luaInterface;
    id_type id;
    BeatClock::id_type beatClockId;
    bool interval;
    int luaFunctionRef = 0;
  };

  BeatClock* beatClock = nullptr;
  containers::IdTable<BeatClockCallbackStateEntry,
                      IdTableContainer,
                      id_type> beatClockCallbackState;

  /*
   * Grid binding state
   */

  MonomeGridInterface* gridInterface;
  int gridOnConnectFunction = 0;
  int gridKeyFunction = 0;

  /*
   * Screen binding state
   */

  GraphicsInterface* graphicsInterface = nullptr;
  int screenTableRef = 0;
  std::array<int, std::variant_size_v<pallet::GraphicsEvent>> graphicsEventStringsRef;

  /*
   * Midi binding state
   */
  MidiInterface* midiInterface = nullptr;

private:
  void setupRequire();
public:
  LuaInterface();
  ~LuaInterface();
  int dostring(const char* str);
  void setClock(Clock& clock);
  void setMonomeGridInterface(MonomeGridInterface& gridInterface);
  void setBeatClock(BeatClock& beatClock);
  void setGraphicsInterface(GraphicsInterface& graphicsInterface);
  void setMidiInterface(MidiInterface& midiInterface);
};

}
