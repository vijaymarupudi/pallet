#pragma once
#include <type_traits>
#include <deque>
#include <array>

#include "lua.hpp"

#include "pallet/containers/IdTable.hpp"
#include "pallet/containers/StaticVector.hpp"

#include "pallet/Platform.hpp"
#include "pallet/ClockInterface.hpp"
#include "pallet/MonomeGridInterface.hpp"
#include "pallet/BeatClock.hpp"
#include "pallet/GraphicsInterface.hpp"
#include "pallet/MidiInterface.hpp"
#include "pallet/OscInterface.hpp"

#include "pallet/error.hpp"

namespace pallet {

// Only the addresses of these values are used
extern const int _luaInterfaceRegistryIndex;
extern const int _palletCTableRegistryIndex;

class LuaInterface {
public:

  using Id = uint32_t;

  // I need pointer stability, cannot use std::vector
  template <class T>
  using IdTableContainer = std::conditional_t<pallet::constants::isEmbeddedDevice,
                                              containers::StaticVector<T, 256>,
                                              std::deque<T>>;

  lua_State* L;

  /*
   * Platform binding state
   */

  Platform* platform = nullptr;

  /*
   * Clock binding state
   */

  struct ClockCallbackStateEntry {
    LuaInterface& luaInterface;
    Id id;
    ClockInterface::Id clockId;
    bool interval;
    int luaFunctionRef = -1;
  };

  ClockInterface* clockInterface = nullptr;
  containers::IdTable<ClockCallbackStateEntry,
                      IdTableContainer,
                      Id> clockCallbackState;

  /*
   * BeatClock binding state
   */

  struct BeatClockCallbackStateEntry {
    LuaInterface& luaInterface;
    Id id;
    BeatClock::Id beatClockId;
    bool interval;
    int luaFunctionRef = -1;
  };

  BeatClock* beatClock = nullptr;
  containers::IdTable<BeatClockCallbackStateEntry,
                      IdTableContainer,
                      Id> beatClockCallbackState;

  /*
   * Grid binding state
   */

  MonomeGridInterface* gridInterface = nullptr;
  int gridOnConnectFunction = 0;
  int gridKeyFunction = 0;

  /*
   * Screen binding state
   */

  GraphicsInterface* graphicsInterface = nullptr;
  int screenOnEventRef = -1;
  std::array<int, pallet::GraphicsEvent::nTypes> graphicsEventStringsRef;

  /*
   * Midi binding state
   */
  MidiInterface* midiInterface = nullptr;

  /*
   * Osc binding state
   */

  OscInterface* oscInterface;

private:
  void setupRequire();
public:
  static Result<LuaInterface> create();
  LuaInterface();
  ~LuaInterface();
  int dostring(const char* str);
  void setPlatform(Platform& platform);
  void setClockInterface(ClockInterface& clock);
  void setMonomeGridInterface(MonomeGridInterface& gridInterface);
  void setBeatClock(BeatClock& beatClock);
  void setGraphicsInterface(GraphicsInterface& graphicsInterface);
  void setMidiInterface(MidiInterface& midiInterface);
  void setOscInterface(OscInterface& oscInterface);
};

}
