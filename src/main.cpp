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
#include "BeatClock.hpp"
#include "GridInterface.hpp"
#include "LuaInterface.hpp"
#include "MidiInterface.hpp"
#include "AudioInterface.hpp"
#include "MidiParser.hpp"
#include <cmath>

int scale(int note) {
  const int s[] = {0, 2, 3, 5, 7, 8, 10};
  return note / 7 * 12 + s[note % 7];
}

void handleInput(char* buf, ssize_t len, pallet::LinuxAudioInterface* iface) {
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

#include <array>
#include <tuple>

int voiceNumber = 0;
int nVoices = 8;

int main() {
  pallet::LinuxPlatform platform;
  platform.init();
  pallet::Clock clock;
  clock.init((pallet::Platform*)&platform);
  pallet::LuaInterface luaInterface;
  luaInterface.init();
  luaInterface.setClock(&clock);
//   luaInterface.dostring(R"(
// local pallet = require("pallet")
// pallet.clock.setInterval((1/60) * 1000 * 1000, function()
//   local now = pallet.clock.currentTime()
//   print(now)
// end)

// )");


  // LinuxAudioInterface audioInterface;
  // audioInterface.init(&clock);

  // LinuxMidiInterface midiInterface;
  // midiInterface.init(&platform);
  // midiInterface.monitor();

  // MidiParser parser;
  // parser.noteOn([](int chan, int note, int vel) {
  //   char buf[1024];
  //   snprintf(buf, 1024, "r%dn%dl%fZ", chan, note, vel ? 1.0 : 0.0);
  //   amy_play_message(buf);
  // });

  // midiInterface.setOnMidi([](uint64_t time, const unsigned char* buf, size_t len, void* ud) {
  //   ((MidiParser*)ud)->uponMidi(buf, len);
  // }, &parser);


  // platform.setFdNonBlocking(0);
  // platform.watchFdIn(0, [](int fd, void* ud){
  //   char buf[8192];
  //   ssize_t len = read(0, buf, 8192);
  //   handleInput(buf, len, (LinuxAudioInterface*)ud);
  // }, &audioInterface);

  // amy_reset_oscs();

  pallet::LinuxMonomeGridInterface gridInterface;
  gridInterface.init(&platform);
  
  gridInterface.setOnConnect([](const std::string& id, bool state, pallet::MonomeGrid* grid, void* ud) {
    grid->setOnKey([](int x, int y, int z, void* ud) {
      printf("%d, %d, %d\n");
    }, nullptr);
    grid->clear();
    grid->led(0, 0, 15);
    grid->led(8, 8, 15);
    grid->led(8, 0, 15);
    grid->led(0, 8, 15);
    grid->render();
  }, nullptr);

  gridInterface.connect(0);

  // gridInterface.setOnKey([](int x, int y, int z, void* ud) {
  //   auto gi = (LinuxMonomeGridInterface*)ud;
  //   gi->clear();
  //   int note = scale(x + y * 16) % 128;
  //   char buf[1024];
  //   if (z == 1) {
  //     snprintf(buf, 1024, "r0n%dl1Z", note);
  //     gi->led(x, y, z * 15);
  //   } else {
  //     snprintf(buf, 1024, "r0dl0Z");
  //     gi->led(x, y, z * 15);
  //   }
  //   amy_play_message(buf);
  //   gi->render();
  // }, &gridInterface);

  // BeatClock beatClock;
  // beatClock.init(&clock, &midiInterface);
  // beatClock.setClockSource(BeatClockType::Midi);
  // // beatClock.sendMidiClock(true);

  // beatClock.setBeatSyncInterval(1, 0, 1.0/2, [](BeatClockEventInfo* info, void* ud) {
  //   printf("tick, %f\n", info->intended);
  // }, nullptr);

  // beatClock.setBeatSyncInterval(1, 1.0/4, 1.0/2, [](BeatClockEventInfo* info, void* ud) {
  //   printf("tock, %f\n", info->intended);
  // }, nullptr);

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
