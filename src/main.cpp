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
  pallet::LinuxPlatform platform;
  pallet::Clock clock(platform);
  pallet::LuaInterface luaInterface;
  pallet::LinuxMidiInterface midiInterface (platform);
  luaInterface.setClock(clock);
  pallet::BeatClock beatClock (clock);
  luaInterface.setBeatClock(beatClock);
  pallet::LinuxGraphicsInterface graphicsInterface(platform);
  luaInterface.setGraphicsInterface(graphicsInterface);
  luaInterface.setMidiInterface(midiInterface);
  
  luaInterface.dostring(R"(
local beatClock = require('pallet').beatClock
local clock = require('pallet').clock
local screen = require('pallet').screen
local midi = require('pallet').midi

local state = false

screen.onEvent = function(event)
  print("Screen:", event.type)
  if (event.type == "MouseMove") then
    if (event.x ~= 0) then
      beatClock.setBPM(event.x)
    end
    print(event.x, ", ", event.y)
  end
end

local actionFunction = function()
  print("beat cb", beatClock.currentBeat())
  state = not state
  local num = 0
  if state then num = 1 end
  midi.sendMidi({144, 60, 127 * num})
  -- screen.rect(0, 0, 30, 30, 15 * num)
  -- screen.render()
end

beatClock.setBeatSyncInterval(1/16, 0, 1/16, actionFunction)
beatClock.setBPM(166)

-- clock.setInterval(clock.timeInMs(25), actionFunction)


)");





  // bool flag = false;
  // int count = 0;

  // std::optional<pallet::ClockIdT> interval;

  // // auto activate = [&]() {
  // //   interval = clock.setInterval([](pallet::ClockEventInfo* cei, void* ud) {
  // //   });
  // // };


  // auto eventCallback = [&](pallet::GraphicsEvent ievent) {
  //   std::visit([&](auto& event) {
  //     if constexpr (std::is_same_v<decltype(event),
  //                          pallet::GraphicsEventKey&>) {
  //       if (event.keycode == ' ' && !event.repeat) {
  //         flag = event.state;
          
  //       }
  //     }
  //   }, ievent);
  // };

  //   // graphicsInterface.setOnEvent([](pallet::GraphicsEvent e, void* u) {
  //   //   static_cast<decltype(eventCallback)*>(u)->operator()(std::move(e));
  //   // }, &eventCallback);

  // graphicsInterface.setOnEvent(make_c_callback<pallet::GraphicsEvent>(eventCallback), &eventCallback);

  
  // auto renderingCallback = [&]() {
  //   graphicsInterface.clear();
    
  //   char buf[1000];
  //   auto len = snprintf(buf, 1000, "%d", count);
  //   // graphicsInterface.text(8, 8, std::string_view(buf, len));

  //   if (flag) {
  //     auto renderText = std::string_view(buf, len);
  //     graphicsInterface.text(15, 15, renderText, 15, 0, pallet::GraphicsPosition::Center, pallet::GraphicsPosition::Bottom);
  //     graphicsInterface.rect(0, 17, 30, 1, 15);
      
  //   } else {
  //     graphicsInterface.rect(0, 0, 30, 30, 0);
  //   }
    
  //   graphicsInterface.render();
  // };

  
  // clock.setInterval(pallet::timeInMs(1000 / 20), [](pallet::ClockEventInfo* cei, void* ud) {
  //   (void)cei;
  //   static_cast<decltype(renderingCallback)*>(ud)->operator()();
  // },
  //   &renderingCallback);

  while (1) {
    platform.loopIter();
  }

  return 0;

  // lua_State* L = luaL_newstate();

  // openlibs(L);
  // openio(L);
  // luaL_dostring(L, "print('hello')");
  // lua_close(L);


  // rtmidiout midiout (RtMidi::Api::UNSPECIFIED, "Vijay Client");
  // midiout.openPort(0);
  // unsigned long interval_time = 20.8333333f * 1000 * 1000;
  // IntervalTimer timer (interval_time);
  // std::vector<unsigned char> midi {0, 0, 0};
  // timer.begin();
  // bool note_status = false;
  // do {
  //   auto time = timer.next();
  //   printf("here %lu %lu\n", time.tv_sec, time.tv_nsec);
  //   time = get_current_time();
  //   printf("actual %lu %lu\n", time.tv_sec, time.tv_nsec);

  //   midi[0] = 0x90;
  //   midi[1] = 0x3C;
  //   midi[2] = note_status ? 0 : 127;
  //   note_status = !note_status;
  //   // midiout.sendMessage(&midi);
  //   midi[0] = 0xF8;
  //   midiout.sendMessage(midi.data(), 1);
  // } while (true);

}
