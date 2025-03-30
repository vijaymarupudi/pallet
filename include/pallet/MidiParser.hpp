#pragma once
#include <functional>

class MidiParser {
public:
  std::function<void(int,int,int)> noteOnFunction = nullptr;
  std::function<void(int,int,int)> ccFunction = nullptr;
  
  void uponMidi(const unsigned char* buf, size_t len) {
    if (len == 3) {
      unsigned char b0 = buf[0];
      unsigned char b1 = buf[1];
      unsigned char b2 = buf[2];
      auto msgType = b0 & 0xF0;
      if (msgType == 0x90 || msgType == 0x80) {
        // note on
        int chan = b0 & 0x0F;
        int note = b1;
        int vel = b2;
        if (msgType == 0x80) {
          vel = 0;
        }
        if (noteOnFunction) {
          noteOnFunction(chan, note, vel);
        }
      }

      if (msgType == 0xB0) {
        ccFunction(b0 & 0x0F, b1, b2);
      }
    } 
  }

  void noteOn(std::function<void(int, int, int)> noteOnFunc) {
    this->noteOnFunction = std::move(noteOnFunc);
  }

  void cc(std::function<void(int, int, int)> ccFunc) {
    this->ccFunction = std::move(ccFunc);
  }
};
