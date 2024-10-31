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


int scale(int note) {
  const int s[] = {0, 2, 4, 5, 7, 9, 11};
  return note / 7 * 12 + s[note % 7];
}

const int RHY_LEN = 32;
bool RHY[RHY_LEN] = {1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1};
int RHY_I = 0;
int RHY_FREQ[RHY_LEN] = {1, 0, 1, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 1, 1};

int voiceNumber = 0;

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

  srand(time(NULL));
  for (int i = 0; i < RHY_LEN; i++) {
    RHY_FREQ[i] = 60 + scale(((float)rand()) / RAND_MAX * 12);
    RHY[i] = ((float)rand()) / RAND_MAX < 0.5;
  }

  amy_reset_oscs();

  char patch[] = ("u1024,"
                  "v2P0.25w0I3.04a0.5,0,0,0.4A0,1,300,0.5,3000,0Z"
                  "v1P0.25w0I1a1,0,0,1A5,1,2000,0,2000,0Z"
                  "v0w8o1O,,,,2,1a1,0,0,0Z");

  amy_play_message((char*)patch);

  char m[8192];

  for (int i = 0; i < 32; i++) {
    snprintf(m, 8192, "r%dK1024Z", i);
    amy_play_message(m);
  }
  
  for (const auto cmd : {
      "h0.9,1Z",
      // "M0.5,500,500Z"
    }) {
    amy_play_message((char*)cmd);  
  }


  
  clock.setInterval(60.0f / 120 / 2 / 2 / 2 * 1000 * 1000, [](void* data){
    char m[8192];
    // snprintf(m, 8192, "v1f%fd%f,0,0,0,0.1l1Z", ((float)rand()) / RAND_MAX * 1000, ((float)rand()) / RAND_MAX * 1);
    if (RHY[RHY_I]) {
      auto time = amy_sysclock();
      snprintf(m, 8192, "r%dn%dl1Z", voiceNumber, RHY_FREQ[RHY_I]);
      amy_play_message(m);
      snprintf(m, 8192, "t%dr%dl0Z", time + 100, voiceNumber);
      amy_play_message(m);
      voiceNumber = (voiceNumber + 1) % 32;
    }

    RHY_I = (RHY_I + 1) % RHY_LEN;

    // ((Clock*)data)->setTimeout(30 * 1000, [](void* data) {
    //   char m[] = "v1l0Z";
    //   amy_play_message(m);
    // }, nullptr);
  }, &clock);


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
