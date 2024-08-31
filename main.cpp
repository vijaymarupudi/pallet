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
  luaInterface.init(&clock);
  luaInterface.dostring(R"(
local pallet = require("pallet")
print(pallet, pallet.clock)
print(require)
print(table)
print(math)
local lt = pallet.clock.currentTime()
local id;
local count = 0
id = pallet.clock.setInterval(10000, function()
  local now = pallet.clock.currentTime()
  print(now)
  lt = now
  count = count + 1
  if count == 100 then
    pallet.clock.clearInterval(id)
  end
end)

)");
  
  // LinuxPlatform platform;
  // platform.init();

  // LinuxMonomeGridInterface gridInterface;
  // gridInterface.init(&platform);
  // gridInterface.setOnConnect(connectCb, &gridInterface);
  // gridInterface.connect();
  // platform.setOnTimer(timerCb, &platform);
  // auto time = platform.currentTime();
  // platform.timer(time + 1 * 500000);

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
