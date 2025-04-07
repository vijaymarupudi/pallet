#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include "pallet/MidiInterface.hpp"

namespace pallet {

void PosixMidiInterface::sendMidi(const unsigned char* buf,
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
  (void)ts;
  auto mface = static_cast<PosixMidiInterface*>(data);
  if (message->size() <= 3) {
    LinuxMidiInterfaceMessage msg;
    for (size_t i = 0; i < message->size(); i++) {
      msg.buf[i] = (*message)[i];
    }
    msg.time = mface->platform.currentTime();
    msg.len = message->size();
    mface->pipe.write(&msg, sizeof(msg));
  }
}

  PosixMidiInterface::PosixMidiInterface(PosixPlatform& platform, Pipe&& ipipe) : platform(platform), readManager(platform), pipe(std::move(ipipe)) {
  status = true;
  platform.setFdNonBlocking(pipe.getReadFd());
  readManager.setFd(pipe.getReadFd());
  readManager.startReading([](int fd, void* data, size_t dlen, void* ud) {
    (void)fd;
    auto mface = static_cast<PosixMidiInterface*>(ud);
    size_t len = dlen / sizeof(LinuxMidiInterfaceMessage);
    auto messages = static_cast<LinuxMidiInterfaceMessage*>(data);
    for (size_t i = 0; i < len; i++) {
      auto& msg = messages[i];
      mface->internalOnMidi(msg.time, msg.buf, msg.len);
    }
  }, this);

  midiIn.setCallback(midiInterfaceMidiInCallback, this);
  midiOut.openVirtualPort("pallet");
  midiIn.ignoreTypes(false, false, false);
  midiIn.openVirtualPort("pallet");
}

void MidiInterface::internalOnMidi(uint64_t time, const unsigned char* buf, size_t len) {
  if (this->monitoring) {
    if (len == 0) { return; }
    printf("midi in | time: %lu, message: ", time);
    printf("0x%02X", buf[0]);
    for (size_t i = 1; i < len; i++) {
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

Result<PosixMidiInterface> PosixMidiInterface::create(PosixPlatform& platform) {
  return pallet::Pipe::create().and_then([&](auto&& pipe) {
    return Result<PosixMidiInterface>(std::in_place_t{}, platform, std::move(pipe));
  });
}

}
