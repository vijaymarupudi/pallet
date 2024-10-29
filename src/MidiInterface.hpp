#include "RtMidi.h"
#include <thread>
#include <sys/socket.h>
#include <iostream>
#include <vector>

class MidiInterface {
public:
  virtual void sendMidi(unsigned char b1,
                         unsigned char b2,
                         unsigned char b3) = 0;
};

class LinuxMidiInterface : public MidiInterface {
  int thread_client_fd;
  int thread_server_fd;
  std::thread thrd;
public:
  LinuxMidiInterface() {};
  void init();
  virtual void sendMidi(unsigned char b1,
                         unsigned char b2,
                         unsigned char b3) override;
};

enum MidiMessageType {
  MIDI_MESSAGE_TYPE_STANDARD = 0
};

void LinuxMidiInterface::sendMidi(unsigned char b1,
                         unsigned char b2,
                         unsigned char b3) {
  unsigned char buf[4] = {MIDI_MESSAGE_TYPE_STANDARD, b1, b2, b3};
  send(thread_client_fd, buf, sizeof(buf), 0);
}

static void midi_interface_midi_in_callback(double ts, std::vector<unsigned char>* message, void* data) {
  int thread_server_fd = reinterpret_cast<intptr_t>(data);
  printf("Msg In: %u", (*message)[0]);
  for (int i = 1; i < message->size(); i++) {
    unsigned char c = (*message)[i];
    printf(", %u", c);
  }
  putc('\n', stdout);
  std::cout << "TID: " << std::this_thread::get_id() << "\n";
  printf("TS: %f\n", ts);
}

static void midi_interface_thread_func(int thread_server_fd) {
  RtMidiIn midiIn;
  midiIn.setCallback(midi_interface_midi_in_callback, (void*)(intptr_t)thread_server_fd);
  midiIn.ignoreTypes(false, false, false);
  midiIn.openPort();
  while (true) {
    unsigned char buf[4];
    int out = recv(thread_server_fd, buf, sizeof(buf), 0);
    std::cout << "TID: " << std::this_thread::get_id() << "\n";
    printf("%u, %u, %u\n", buf[1], buf[2], buf[3]);
  }
}

void LinuxMidiInterface::init() {
  int sockets[2];
  thread_server_fd = socketpair(AF_LOCAL, SOCK_STREAM, 0, sockets);
  thread_client_fd = sockets[0];
  thread_server_fd = sockets[1];
  thrd = std::thread(midi_interface_thread_func, thread_server_fd);
}
