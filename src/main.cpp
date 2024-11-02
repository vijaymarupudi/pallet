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

struct Gridder {
  LinuxMonomeGridInterface& gridInterface;
  Clock& clock;
  float t;
  float speed = 1;
  int prevKey = -1;
  bool begun = false;
  void begin() {
    if (begun) { return; }
    amy_play_message("v0w1A300,0.5,1000,1,300,0.5,1000,0M0.5,500,500,0.8h0.5,0.9Z");
    gridInterface.setOnKey([](int x, int y, int z, void* ud) {
      auto gridder = (Gridder*)ud;
      gridder->speed = 1 + (x * 4);
      char message[16];
      int note = scale(x + y * 16) % 128;
      snprintf(message, 16, "v0n%dl%dZ", note, z);
      if (z == 0) {
        if (gridder->prevKey == note) {
          amy_play_message(message);
        }
      } else {
        amy_play_message(message);
        gridder->prevKey = note;
      }


      // if (z == 1) {
      //   amy_play_message("v0l1Z");
      // } else {
      //   amy_play_message("v0l0Z");
      // }
      // printf("%d, %d, %d\n", x, y, z);
    }, this);
    begun = true;
    t = 0;
    this->step();
    clock.setInterval(1.0f / 120 * 1000 * 1000, [](void* ud) {
      auto gridder = ((Gridder*)ud);
      gridder->step();
    }, this);
  }
  void step() {
    t = t + (1.0f / 120) * speed;
    gridInterface.clear();
    for (int i = 0; i < 16 * 16; i++) {
      int x = i % 16;
      int y = i / 16;
      gridInterface.led(x, y, ((sin(t + 2 * (x / 16.0f * 2 + 1) * (y / 16.0f * 4 + 1)) + 1) / 2) * 15);
    }
    gridInterface.render();

  }
};

#include <termios.h>

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


  int fd = open("/dev/ttyACM0", O_NONBLOCK | O_RDWR);

  if (fd < 0) {
    perror("Error: ");
    exit(1);
  }

  struct termios params;
  tcgetattr(fd, &params);
  cfsetospeed(&params, 115200);
  cfsetispeed(&params, 115200);
  /* parity (8N1) */
  params.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
  params.c_cflag |=  (CS8 | CLOCAL | CREAD);

  /* no line processing */
  params.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN);

  /* raw input */
  params.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK |
                  INPCK | ISTRIP | IXON);

  /* raw output */
  params.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR |
                  OFILL | OPOST);

  params.c_cc[VMIN]  = 1;
  params.c_cc[VTIME] = 0;

  tcsetattr(fd, TCSANOW, &params);

  platform.watchFdIn(fd, [](int fd, void* ud) {
    char buf[1024];
    ssize_t len = read(fd, buf, 1024);

    if (len < 0) {
      perror("Error: ");
      exit(1);
    }

    if (len % 3 != 0) {
      fprintf(stderr, "Error in reading grid serial\n");
    }

    for (int i = 0; i < len; i += 3) {
      unsigned char b1 = buf[i];
      unsigned char b2 = buf[i + 1];
      unsigned char b3 = buf[i + 2];

      char buf[1024];


      if (b1 == (2 << 4) + 0x00) {
        snprintf(buf, 1024, "v%dl0Z", voiceNumber);
        amy_play_message(buf);
        // voiceNumber = (voiceNumber + 1) % nVoices;
      }

      if (b1 == (2 << 4) + 0x01) {
        snprintf(buf, 1024, "v%dl1n%dZ", voiceNumber, scale(b2 + b3 * 16) % 127);
        amy_play_message(buf);
      }
    }
  }, nullptr);


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
