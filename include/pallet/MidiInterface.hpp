#pragma once

#include "RtMidi.h"
#include "pallet/error.hpp"

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

}

#include "pallet/PosixPlatform.hpp"
#include "pallet/posix.hpp"

namespace pallet {

class PosixMidiInterface final : public MidiInterface {
public:
  bool status = false;
  RtMidiIn midiIn;
  RtMidiOut midiOut;
  PosixPlatform& platform;
  FdManager readManager;
  Pipe pipe;

  PosixMidiInterface(PosixPlatform& platform, Pipe&& ipipe);
  virtual void sendMidi(const unsigned char* buf,
                        size_t len) override;
  static Result<PosixMidiInterface> create(PosixPlatform& platform);
};

}
