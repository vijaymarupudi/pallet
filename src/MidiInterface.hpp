#include "RtMidi.h"
#include <sys/socket.h>
#include <iostream>
#include <vector>

using MidiInterfaceOnMidiCbT = void(*)(uint64_t, const unsigned char*, size_t, void*);

class MidiInterface {
public:
  MidiInterfaceOnMidiCbT onMidiCb = nullptr;
  void* onMidiUserData = nullptr;
  MidiInterfaceOnMidiCbT onMidiClockCb = nullptr;
  void* onMidiClockUserData = nullptr;
  bool monitoring = false;
  void setOnMidi(MidiInterfaceOnMidiCbT cb, void* ud) {
    this->onMidiCb = cb;
    this->onMidiUserData = ud;
  }
  void setOnMidiClock(MidiInterfaceOnMidiCbT cb, void* ud) {
    this->onMidiClockCb = cb;
    this->onMidiClockUserData = ud;
  }

  void monitor(bool state = true) {
    this->monitoring = state;
  }

  void internalOnMidi(uint64_t time, const unsigned char* buf, size_t len, void* ud) {
    if (this->monitoring) {
      if (len == 0) { return; }
      printf("midi in | time: %lu, message: ");
      printf("0x%02X", buf[0]);
      for (int i = 1; i < len; i++) {
        printf(", 0x%02X", buf[i]);
      }
      printf("\n");
    }
    if (this->onMidiClockCb) {
      this->onMidiClockCb(time, buf, len, this->onMidiUserData);
    }
    if (this->onMidiCb) {
      this->onMidiCb(time, buf, len, this->onMidiUserData);
    }
  }
  
  virtual void sendMidi(const unsigned char* buf, size_t len) = 0;
  
  
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
  virtual void sendMidi(const unsigned char* buf,
                        size_t len) override;
};

void LinuxMidiInterface::sendMidi(const unsigned char* buf,
                                  size_t len) {
  midiOut.sendMessage(buf, len);
}

struct LinuxMidiInterfaceMessage {
  uint64_t time;
  unsigned char buf[3];
  unsigned char len;
};

static void midiInterfaceMidiInCallback(double ts,
                                        std::vector<unsigned char>* message,
                                        void* data) {
  auto mface = reinterpret_cast<LinuxMidiInterface*>(data);
  if (message->size() <= 3) {
    LinuxMidiInterfaceMessage msg;
    for (int i = 0; i < message->size(); i++) {
      msg.buf[i] = (*message)[i];
    }
    msg.time = mface->platform->currentTime();
    msg.len = message->size();
    write(mface->threadWriteFd, &msg, sizeof(msg));
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
    LinuxMidiInterfaceMessage messages[16];
    ssize_t len = read(fd, &messages[0],
                       16 * sizeof(LinuxMidiInterfaceMessage)) /
      sizeof(LinuxMidiInterfaceMessage);
    for (int i = 0; i < len; i += 1) {
      auto& msg = messages[i];
      mface->internalOnMidi(msg.time, msg.buf, msg.len, mface->onMidiUserData);
    }
  }, this);
  midiIn.setCallback(midiInterfaceMidiInCallback, this);
  midiOut.openPort();
  midiIn.ignoreTypes(false, false, false);
  midiIn.openPort();
}

