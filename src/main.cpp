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
#include "Clock.hpp"
#include "GridInterface.hpp"
#include "LuaInterface.hpp"
#include "MidiInterface.hpp"
#include "AudioInterface.hpp"
#include <cmath>

int scale(int note) {
  const int s[] = {0, 2, 3, 5, 7, 8, 10};
  return note / 7 * 12 + s[note % 7];
}

void handleInput(char* buf, ssize_t len, LinuxAudioInterface* iface) {
  int processed = 0;
  for (int i = 0; i < 8192; i++) {
      if (buf[i] == '\n') {
        buf[i] = 0;
        if (strcmp(buf, "r") == 0) {
          iface->toggleRecord();
        } else {
          amy_play_message(buf);
        }

        processed = i + 1;
        break;
      }
  }

  if (processed != len) {
    return handleInput(buf + processed, len - processed, iface);
  }

}

int voiceNumber = 0;
int nVoices = 16;

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

  // LinuxMonomeGridInterface gridInterface;
  // auto gridder = Gridder(gridInterface, clock);
  // gridInterface.init(&platform);
  // gridInterface.setOnConnect([](const std::string& id, bool connected, void* ud){
  //   if (connected)
  //     {
  //       printf("connected %s!\n", id.c_str());
  //       ((Gridder*)ud)->begin();
  //     }
  //   else
  //     { printf("disconnected %s!\n", id.c_str()); }
  // }, &gridder);
  // gridInterface.connect();

  LinuxAudioInterface audioInterface;
  audioInterface.init(&clock);

  // LinuxMonomeGridInterface gridInterface;
  // gridInterface.init(&platform);
  // gridInterface.setOnConnect(connectCb, &gridInterface);
  // gridInterface.connect();
  // platform.setOnTimer(timerCb, &platform);
  // auto time = platform.currentTime();
  // platform.timer(time + 1 * 500000);

  LinuxMidiInterface midiInterface;
  midiInterface.setOnMidi([](unsigned char* buf, size_t len, void* ud) {
    for (int i = 0; i < len; i++) {
      printf("0x%02X ", buf[i]);
    }
    printf("\n");
  }, nullptr);
  midiInterface.init(&platform);

  platform.setFdNonBlocking(0);
  platform.watchFdIn(0, [](int fd, void* ud){
    char buf[8192];
    ssize_t len = read(0, buf, 8192);
    handleInput(buf, len, (LinuxAudioInterface*)ud);
  }, &audioInterface);

  amy_reset_oscs();

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
