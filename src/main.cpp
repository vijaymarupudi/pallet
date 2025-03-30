#include <stdio.h>
#include "pallet/LinuxPlatform.hpp"
#include "pallet/Clock.hpp"
#include "pallet/PosixGraphicsInterface.hpp"
#include "pallet/LuaInterface.hpp"
#include "pallet/BeatClock.hpp"
#include "pallet/OscMonomeGridInterface.hpp"
#include "pallet/LoOscInterface.hpp"

template <class... Args>
auto make_c_callback(auto& lambda) {
  auto c_callback = [](Args... args, void* ud) {
    return static_cast<std::remove_reference_t<decltype(lambda)>*>(ud)->operator()(args...);
  };
  return +c_callback;
}

int main() {
  auto platformResult = pallet::LinuxPlatform::create();
  if (!platformResult) return 1;
  auto clockResult = pallet::Clock::create(*platformResult);
  auto midiInterfaceResult = pallet::PosixMidiInterface::create(*platformResult);
  auto beatClockResult = pallet::BeatClock::create(*clockResult);
  auto graphicsInterfaceResult = pallet::PosixGraphicsInterface::create(*platformResult);
  auto oscInterfaceResult = pallet::LoOscInterface::create(*platformResult);
  if (!oscInterfaceResult) return 1;
  auto gridInterfaceResult = pallet::OscMonomeGridInterface::create(*oscInterfaceResult);
  auto luaInterfaceResult = pallet::LuaInterface::create();

  if (!(platformResult && clockResult && midiInterfaceResult && beatClockResult && graphicsInterfaceResult && oscInterfaceResult && gridInterfaceResult && luaInterfaceResult)) {
    return 1;
  }
  
//   auto& luaInterface = *luaInterfaceResult;
//   luaInterface.setBeatClock(*beatClockResult);
//   luaInterface.setClock(*clockResult);
//   luaInterface.setGraphicsInterface(*graphicsInterfaceResult);
//   luaInterface.setMidiInterface(*midiInterfaceResult);
//   luaInterface.setMonomeGridInterface(*gridInterfaceResult);

//   luaInterface.dostring(R"(
// local beatClock = require('pallet').beatClock
// local clock = require('pallet').clock
// local screen = require('pallet').screen
// local midi = require('pallet').midi
// local grid = require('pallet').grid

// local state = false

// local originx = 1
// local originy = 1

// local TEXT = ""

// local function text(x, y, string, c)
//   screen.text(x, y, string, c, 0, screen.Default, screen.Default)
// end

// grid.connect(1, function(g)
//   g:setOnKey(function(x, y, z)
//     TEXT = string.format("%d %d %d", x, y, z)
//   end)
// end)

// clock.setInterval(clock.timeInMs(math.floor(1/60 * 1000)), function()
//   screen.clear()
//   text(30, 30, TEXT, 15)
//   screen.render()
// end)

// -- screen.setOnEvent(function(event)
// --   if (event.type == "MouseMove") then
// --     if (event.x ~= 0) then
// --       beatClock.setBPM(event.x * 2)
// --     end
// --     originx = event.x
// --     originy = event.y
// --   elseif event.type == "Quit" then
// --     screen.quit()
// --   end
// -- end)

// -- clock.setInterval(clock.timeInMs(25), actionFunction)

// )");

  auto& platform = *platformResult;
  while (1) {
    platform.loopIter();
  }

  return 0;


}
