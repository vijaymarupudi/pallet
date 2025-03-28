#include <stdio.h>
#include "Platform.hpp"
#include "Clock.hpp"
#include "GraphicsInterface.hpp"
#include "LuaInterface.hpp"
#include "BeatClock.hpp"

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

  auto midiInterfaceResult = pallet::LinuxMidiInterface::create(platform);

  if (!midiInterfaceResult) {
    return 1;
  }

  auto& midiInterface = *midiInterfaceResult;

  auto luaInterfaceResult = pallet::LuaInterface::create();

  if (!luaInterfaceResult) {
    return 1;
  }

  auto& luaInterface = *luaInterfaceResult;

  luaInterface.setClock(clock);
  auto beatClockResult = pallet::BeatClock::create(clock);
  if (!beatClockResult) {
    return 1;
  }

  auto& beatClock = *beatClockResult;
  luaInterface.setBeatClock(beatClock);

  auto graphicsInterfaceResult = pallet::LinuxGraphicsInterface::create(platform);
  if (!graphicsInterfaceResult) {
    return 1;
  }

  auto& graphicsInterface = *graphicsInterfaceResult;
  // auto graphicsInterface = *std::move(graphicsInterfaceResult);

  // auto another = pallet::LinuxGraphicsInterface::create(platform);
  // graphicsInterface = *std::move(another);

  // pallet::SDLHardwareInterface iface;
  // iface.init();
  // SDL_Delay(2000);

  // SDL_Init(SDL_INIT_VIDEO);
  // auto window = SDL_CreateWindow("Testing", 1080, 720, 0);
  // auto screenSurface = SDL_GetWindowSurface(window);
  // SDL_FillRect(screenSurface, NULL, SDL_MapRGB(screenSurface->format, 0xFF, 0xFF, 0xFF));
  // SDL_UpdateWindowSurface(window);
  // printf("%p: HERE\n", (void*)window);
  // SDL_DestroyWindow(window);

  luaInterface.setGraphicsInterface(graphicsInterface);
  luaInterface.setMidiInterface(midiInterface);

  luaInterface.dostring(R"(
local beatClock = require('pallet').beatClock
local clock = require('pallet').clock
local screen = require('pallet').screen
local midi = require('pallet').midi

local state = false

screen.setOnEvent(function(event)
  print("Screen:", event.type)
  if (event.type == "MouseMove") then
    if (event.x ~= 0) then
      beatClock.setBPM(event.x * 2)
    end
    print(event.x, ", ", event.y)
  end
end)

local actionFunction = function()
  print("beat cb", beatClock.currentBeat())
  state = not state
  local num = 0
  if state then num = 1 end
  midi.sendMidi({144, 60, 127 * num})
  screen.rect(0, 0, 30, 30, 15 * num)
  screen.render()
end

beatClock.setBeatSyncInterval(1/16, 0, 1/16, actionFunction)
beatClock.setBPM(166)

-- clock.setInterval(clock.timeInMs(25), actionFunction)


)");



  while (1) {
    platform.loopIter();
  }

  return 0;


}
