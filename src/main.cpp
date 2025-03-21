#include <stdio.h>
#include <sys/timerfd.h>
#include <inttypes.h>
#include <unistd.h>
#include <sched.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include "lua.hpp"
#include "Platform.hpp"
#include "Clock.hpp"
#include "GraphicsInterface.hpp"
#include <cmath>
#include <array>
#include <tuple>

int main() {
  pallet::LinuxPlatform platform;
  pallet::Clock clock(platform);
  pallet::LinuxGraphicsInterface graphicsInterface(platform);

  struct State {
    bool flag = false;
    int count = 0;
    pallet::LinuxGraphicsInterface* graphicsInterface;
  };

  State state { false, 0, &graphicsInterface };

  graphicsInterface.setOnEvent([](pallet::GraphicsEvent ievent, void* u) {
    auto& state = *static_cast<State*>(u);
    auto& flag = state.flag;
    std::visit([&](auto& event) {
      if constexpr (std::is_same_v<decltype(event), pallet::GraphicsEventMouseButton&>) {
        if (event.state) {
          flag = !flag;
        }
        printf("%d, %d, %d, %d\n", event.x, event.y, event.button, event.state);
      } else if constexpr (std::is_same_v<decltype(event), pallet::GraphicsEventKey&>) {
        if (event.keycode == ' ') {
          flag = event.state;
        }

        if (event.repeat) {
          state.count++;
        }
        printf("Key event! %c %d %d\n", event.keycode, event.state, event.repeat);
      }
    }, ievent);
  }, &state);

  
  clock.setInterval(pallet::timeInMs(1000 / 20), [](pallet::ClockEventInfo* cei, void* ud) {
    (void)ud;
    (void)cei;

    auto state = (State*)ud;

    state->graphicsInterface->clear();

    
    char buf[1000];
    auto len = snprintf(buf, 1000, "%d", state->count);
    // state->graphicsInterface->text(8, 8, std::string_view(buf, len));

    if (state->flag) {
      auto renderText = std::string_view(buf, len);
      state->graphicsInterface->text(15, 15, renderText, 15, 0, pallet::GraphicsPosition::Center, pallet::GraphicsPosition::Bottom);
      state->graphicsInterface->rect(0, 17, 30, 1, 15);
      
    } else {
      state->graphicsInterface->rect(0, 0, 30, 30, 0);
    }
    
    state->graphicsInterface->render();
  }, &state);

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
