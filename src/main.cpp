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
#include "MonomeGridInterface.hpp"
#include "LuaInterface.hpp"
#include "MidiInterface.hpp"
#include "AudioInterface.hpp"
#include "MidiParser.hpp"
#include <cmath>
#include <array>
#include <tuple>

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

int voiceNumber = 0;
int nVoices = 8;

int main() {
  pallet::LinuxPlatform platform;
  pallet::Clock clock(platform);
  pallet::LinuxMonomeGridInterface gridInterface(platform);
  pallet::LuaInterface luaInterface;
  luaInterface.setClock(clock);
  luaInterface.setMonomeGridInterface(gridInterface);
  
  int d = luaInterface.dostring(R"(
local clock = require("pallet").clock
local grid = require("pallet").grid
grid.connect(1, function(g)
  g:setOnKey(function(x, y, z)
  print(x, y, z)
  g:clear()
  g:led(x, y, z * 15)
  g:render()    
  end)
  local state = 0
  clock.setInterval(50000000, function()
    g:clear()
    if state == 0 then state = 1 else state = 0 end
    g:led(1, 1, state * 15)
    g:render()
  end)
end)



)");

  if (d) {
    printf("msg: %s\n", lua_tostring(luaInterface.L, -1));
  }

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
