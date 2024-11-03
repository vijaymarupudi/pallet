#include "RtMidi.h"
#include <sys/socket.h>
#include <iostream>
#include <vector>

class MidiInterface {
public:
  void(*onMidiCb)(unsigned char*, size_t, void*) = nullptr;
  void* onMidiUserData = nullptr;
  void setOnMidi(void(*cb)(unsigned char*, size_t, void*), void* ud) {
    this->onMidiCb = cb;
    this->onMidiUserData = ud;
  }
  virtual void sendMidi(unsigned char* buf, size_t len) = 0;
  
};

#include <unistd.h>
#include <stdlib.h>

class LinuxMidiInterface : public MidiInterface {
public:
  bool status = false;
  int threadReadFd;
  int threadWriteFd;
  RtMidiIn midiIn;
  RtMidiOut midiOut;
  LinuxPlatform* platform;

  LinuxMidiInterface() {};
  void init(LinuxPlatform* platform);
  virtual void sendMidi(unsigned char* buf,
                        size_t len) override;
};

void LinuxMidiInterface::sendMidi(unsigned char* buf,
                                  size_t len) {
  midiOut.sendMessage(buf, len);
}

static void midiInterfaceMidiInCallback(double ts,
                                        std::vector<unsigned char>* message,
                                        void* data) {
  auto mface = reinterpret_cast<LinuxMidiInterface*>(data);
  if (message->size() <= 3) {
    unsigned char buf[4] = {0, 0, 0, 0};
    for (int i = 0; i < message->size(); i++) {
      buf[i + 1] = (*message)[i];
    }
    buf[0] = message->size();
    write(mface->threadWriteFd, buf, sizeof(buf));
  }
}

void LinuxMidiInterface::init(LinuxPlatform* platform) {
  status = true;
  this->platform = platform;
  int fds[2];
  pipe(fds);
  threadReadFd = fds[0];
  threadWriteFd = fds[1];
  platform->setFdNonBlocking(threadReadFd);
  platform->watchFdIn(threadReadFd, [](int fd, void* ud) {
    auto mface = (LinuxMidiInterface*)ud;
    unsigned char buf[8192];
    ssize_t len = read(fd, buf, 8192);
    if (len % 4 != 0) {
      fprintf(stderr, "midi error!\n");
      exit(1);
    }
    for (int i = 0; i < len; i += 4) {
      if (!mface->status) { break; }
      size_t len = buf[i];
      unsigned char* msg = &buf[i + 1];
      if (mface->onMidiCb) {
        mface->onMidiCb(msg, len, mface->onMidiUserData);
      }
    }
  }, this);
  midiIn.setCallback(midiInterfaceMidiInCallback, this);
  midiOut.openPort();
  midiIn.ignoreTypes(false, false, false);
  midiIn.openPort();
}

