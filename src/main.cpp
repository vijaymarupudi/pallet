#include <stdio.h>
#include <sys/timerfd.h>
#include <inttypes.h>
#include <unistd.h>
#include <sched.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include "RtMidi.h"
#include "lua.hpp"
#include <stdlib.h>
#include <unistd.h>
#include "Clock.hpp"
#include "GridInterface.hpp"
#include "LuaInterface.hpp"
#include "MidiInterface.hpp"
#include "AudioInterface.hpp"
#include <cmath>

static void keyCb(int x, int y, int z, void* data) {
  auto interface = ((MonomeGridInterface*)data);
  interface->led(x, y, z * 15);
  interface->render();
  printf("%d, %d, %d\n", x, y, z);
}

static void connectCb(void* data) {
  printf("Connected!\n");
  auto gridInterface = ((MonomeGridInterface*)data);
  gridInterface->setOnKey(keyCb, gridInterface);
}

static void intervalCb(void* data) {
  printf("Hello world! %llu\n", ((Clock*)data)->currentTime());
}

static void timerCb(void* data) {
  printf("WOW\n");
}

int main() {
  LinuxPlatform platform;
  platform.init();
  Clock clock;
  clock.init((Platform*)&platform);
  LuaInterface luaInterface;
  luaInterface.init();
  luaInterface.setClock(&clock);
//   luaInterface.dostring(R"(
// local pallet = require("pallet")
// pallet.clock.setInterval((1/60) * 1000 * 1000, function()
//   local now = pallet.clock.currentTime()
//   print(now)
// end)

// )");

  LinuxAudioInterface audioInterface;

  audioInterface.init();

  // LinuxMonomeGridInterface gridInterface;
  // gridInterface.init(&platform);
  // gridInterface.setOnConnect(connectCb, &gridInterface);
  // gridInterface.connect();
  // platform.setOnTimer(timerCb, &platform);
  // auto time = platform.currentTime();
  // platform.timer(time + 1 * 500000);

  // clock.setInterval(50 * 1000, [](void* data) {
  //   // bleep();
  //   auto e = amy_default_event();
  //   e.osc = 0;
  //   e.freq_coefs[COEF_CONST] = (int)(((float)rand()) / RAND_MAX * 1000 + 100);
  //   e.velocity = 1;
  //   e.wave = PULSE;
  //   amy_add_event(e);
  //   e.osc = 0;
  //   e.time = amy_sysclock() + (int)(((float)rand()) / RAND_MAX * 100 + 10);
  //   e.freq_coefs[COEF_CONST] = 220;
  //   e.velocity = 0;
  //   amy_add_event(e);
    
  //   // ((Clock*)data)->setTimeout(80 * 1000, [](void* data) {
  //   //   printf("TIMEOUT!\n");
  //   //   char m2[] = "v0f220.0l0.0Z";
  //   //   amy_play_message(m2);
  //   // }, nullptr);
  // }, &clock);

  clock.setInterval(100 * 1000, [](void* data){ bleep(); }, nullptr);
  

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
