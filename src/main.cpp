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

  pallet::SDLThreadedInterface sdlInterface(platform);

  struct S {
    int count = 0;
    pallet::SDLThreadedInterface* sdlInterface;
  };
  
  S state { 0, &sdlInterface };  

  clock.setInterval(pallet::timeInS(1), [](pallet::ClockEventInfo* cei, void* ud) {
    auto state = (S*)ud;
    state->count += 1;
    (void)ud;
    (void)cei;
    // state->sdlInterface->text(0, 0, "text!", 5);
    state->sdlInterface->rect(0, 0, 10, 10, 15);
    state->sdlInterface->render();
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
