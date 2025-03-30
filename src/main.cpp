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
  if (!platformResult) {
    return 1;
  }

  auto& platform = *platformResult;

  auto clockResult = pallet::Clock::create(platform);
  if (!clockResult) {
    return 1;
  }
  auto& clock = *clockResult;

  auto midiInterfaceResult = pallet::PosixMidiInterface::create(platform);

  if (!midiInterfaceResult) {
    return 1;
  }

  auto& midiInterface = *midiInterfaceResult;


  auto beatClockResult = pallet::BeatClock::create(clock);
  if (!beatClockResult) {
    return 1;
  }

  auto& beatClock = *beatClockResult;
  
  auto graphicsInterfaceResult = pallet::PosixGraphicsInterface::create(platform);
  if (!graphicsInterfaceResult) {
    return 1;
  }

  auto& graphicsInterface = *graphicsInterfaceResult;
  // auto graphicsInterface = *std::move(graphicsInterfaceResult);

  auto oscInterfaceResult = pallet::LoOscInterface::create(platform);
  if (!oscInterfaceResult) {
    return 1;
  }
  auto gridInterfaceResult = pallet::OscMonomeGridInterface::create(*oscInterfaceResult);
  if (!gridInterfaceResult) {
    return 1;
  }

    auto luaInterfaceResult = pallet::LuaInterface::create();

  if (!luaInterfaceResult) {
    return 1;
  }

  auto& luaInterface = *luaInterfaceResult;
  luaInterface.setBeatClock(beatClock);
  luaInterface.setClock(clock);
  luaInterface.setGraphicsInterface(graphicsInterface);
  luaInterface.setMidiInterface(midiInterface);
  luaInterface.setMonomeGridInterface(*gridInterfaceResult);

  luaInterface.dostring(R"(
local beatClock = require('pallet').beatClock
local clock = require('pallet').clock
local screen = require('pallet').screen
local midi = require('pallet').midi
local grid = require('pallet').grid

local state = false

local originx = 1
local originy = 1

local TEXT = ""

local function text(x, y, string, c)
  screen.text(x, y, string, c, 0, screen.Default, screen.Default)
end

grid.connect(1, function(g)
  g:setOnKey(function(x, y, z)
    TEXT = string.format("%d %d %d", x, y, z)
  end)
end)

clock.setInterval(clock.timeInMs(math.floor(1/60 * 1000)), function()
  screen.clear()
  text(30, 30, TEXT, 15)
  screen.render()
end)

-- screen.setOnEvent(function(event)
--   if (event.type == "MouseMove") then
--     if (event.x ~= 0) then
--       beatClock.setBPM(event.x * 2)
--     end
--     originx = event.x
--     originy = event.y
--   elseif event.type == "Quit" then
--     screen.quit()
--   end
-- end)

-- clock.setInterval(clock.timeInMs(25), actionFunction)

)");

  while (1) {
    platform.loopIter();
  }

  return 0;


}
