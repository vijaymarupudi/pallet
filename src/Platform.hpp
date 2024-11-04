#pragma once
#include <inttypes.h>

class Platform {
protected:
  void (*timerCb)(void*) = nullptr;
  void* timerCbUserData;
  public:
  void setOnTimer(void (*cb)(void*), void* userData) {
    this->timerCb = cb;
    this->timerCbUserData = userData;
  }
  // should be thread safe!
  virtual uint64_t currentTime() = 0;
  virtual void timer(uint64_t time) = 0;
};

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

#include <map>
#include <utility>
#include <tuple>
#include <poll.h>
#include <time.h>
#include <fcntl.h>
#include <sys/resource.h>

class LinuxPlatform : public Platform {
public:
  int cpu_dma_latency_fd;
  int timerfd;
  struct pollfd pollFds[16];
  std::map<int, std::tuple<int, void(*)(int, void*), void*>> fdCallbacks;
  void init();
  virtual uint64_t currentTime() override;
  virtual void timer(uint64_t time) override;
  void uponTimer();
  void watchFdIn(int fd, void (*callback)(int, void*), void* userData);
  void removeFd(int fd);
  void loopIter();
  void cleanup();
  void setFdNonBlocking(int fd);
};


#endif
