#pragma once
#include "RtMidi.h"
#include "Platform.hpp"

namespace pallet {

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

  void internalOnMidi(uint64_t time, const unsigned char* buf, size_t len);  
  virtual void sendMidi(const unsigned char* buf, size_t len) = 0;
  
  
};

class LinuxMidiInterface : public MidiInterface {
public:
  bool status = false;
  int threadReadFd;
  int threadWriteFd;
  RtMidiIn midiIn;
  RtMidiOut midiOut;
  LinuxPlatform& platform;
  LinuxMidiInterface(LinuxPlatform& platform);
  virtual void sendMidi(const unsigned char* buf,
                        size_t len) override;
};

}
