#include <unistd.h>
#include <stdlib.h>
#include <vector>
#include "MidiInterface.hpp"

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
  midiOut.openVirtualPort("pallet");
  midiIn.ignoreTypes(false, false, false);
  midiIn.openVirtualPort("pallet");
}

