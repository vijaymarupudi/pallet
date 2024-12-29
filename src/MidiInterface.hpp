#pragma once
#include "RtMidi.h"
#include "Platform.hpp"

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
      printf("midi in | time: %lu, message: ", time);
      printf("0x%02X", buf[0]);
      for (int i = 1; i < len; i++) {
        printf(", 0x%02X", buf[i]);
      }
      printf("\n");
    }
    if (this->onMidiClockCb) {
      this->onMidiClockCb(time, buf, len, this->onMidiClockUserData);
    }
    if (this->onMidiCb) {
      this->onMidiCb(time, buf, len, this->onMidiUserData);
    }
  }
  
  virtual void sendMidi(const unsigned char* buf, size_t len) = 0;
  
  
};

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
