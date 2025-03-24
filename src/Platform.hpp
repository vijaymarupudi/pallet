#pragma once
#include <inttypes.h>

#include "error.hpp"

namespace pallet {

class Platform {
protected:
  void (*timerCb)(void*) = nullptr;
  void* timerCbUserData;
  public:
  void setOnTimer(void (*cb)(void*), void* userData) {
    this->timerCb = cb;
    this->timerCbUserData = userData;
  }
  // Required to be thread safe!
  virtual uint64_t currentTime() = 0;
  virtual void timer(uint64_t time, bool off = false) = 0;
};

}


#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

#include <map>

#include <poll.h>
#include <time.h>

namespace pallet {

class LinuxPlatform : public Platform {

  struct timespec referenceTime;

  using FdCallback = void(*)(int fd, int revents, void* ud);

  struct FdPollState {
    int events;
    FdCallback callback;
    void* ud;
  };

  FdPollState& findOrCreateFdPollState(int fd);
  
public:
  static constexpr int Read  = POLLIN;
  static constexpr int Write = POLLOUT;
  int cpu_dma_latency_fd;
  int timerfd;
  struct pollfd pollFds[32];
  std::map<int, FdPollState> fdCallbacks;
  void uponTimer();

  static Result<LinuxPlatform> create();
  LinuxPlatform();
  ~LinuxPlatform();
  void watchFdIn(int fd, FdCallback callback, void* userData);
  void watchFdOut(int fd, FdCallback callback, void* userData);
  void watchFdEvents(int fd, int events, FdCallback callback, void* userData);
  void unwatchFdIn(int fd);
  void unwatchFdOut(int fd);
  void unwatchFdEvents(int fd, int events);
  void removeFd(int fd);
  void loopIter();
  void cleanup();
  void setFdNonBlocking(int fd);

  virtual uint64_t currentTime() override;
  virtual void timer(uint64_t time, bool off = false) override;
};

class FdManager {

  using WriteCallback = void (*)(int fd, void* ud);
  using ReadCallback = void (*)(int fd, void* data, size_t len, void* ud);
  
  struct WriteState {
    void* data;
    size_t len;
    size_t writtenLen;
    WriteCallback cb;
    void* ud;
  };

  struct ReadState {
    ReadCallback cb;
    void* ud;
  };

  LinuxPlatform& platform;
  WriteState writeState;
  ReadState readState;
  int fd;

  void uponWriteReady();
  void uponReadReady();
  void uponReady(int revents);

public:

  FdManager(LinuxPlatform& platform, int fd = -1);
  void setFd(int fd);
  void write(void* data, size_t len, WriteCallback cb, void* ud);
  void startReading(ReadCallback cb, void* ud);
  void stopReading();
  ~FdManager();

  static void platformCallback(int fd, int revents, void* ud);
};



#endif

}
