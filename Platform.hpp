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
  virtual uint64_t currentTime() = 0;
  virtual void timer(uint64_t time) = 0;
};

#ifdef __linux__
#include <map>
#include <utility>
#include <poll.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <time.h>

static void time_to_timespec(uint64_t time, struct timespec* spec) {
  time_t s = time / 1000000;
  time_t ns = (time % 1000000) * 1000;
  spec->tv_sec = s;
  spec->tv_nsec = ns;
}

static uint64_t timespec_to_time(struct timespec* spec) {
  return (spec->tv_sec * 1000000) + (spec->tv_nsec / 1000);
}

static void timer_callback(int fd, void* data);

class LinuxPlatform : public Platform {
public:
  int timerfd;
  struct pollfd pollFds[16];
  std::map<int, std::pair<void(*)(int, void*), void*>> fdCallbacks;

  void init() {
    timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
    fdCallbacks[timerfd] = std::pair(timer_callback, this);
  }

  virtual uint64_t currentTime() override {
    struct timespec spec;
    clock_gettime(CLOCK_MONOTONIC, &spec);
    return timespec_to_time(&spec);
  }

  virtual void timer(uint64_t time) {
    // 0 to disarm
    struct timespec it_interval = {0};
    struct timespec it_value = {0};
    if (time != 0) {
      time_to_timespec(time, &it_value);  
    }
    struct itimerspec its = {it_interval, it_value};
    timerfd_settime(this->timerfd, TFD_TIMER_ABSTIME, &its, nullptr);
  }

  void uponTimer() {
    uint64_t expirations;
    read(this->timerfd, &expirations, sizeof(expirations));
    if (this->timerCb) {
      this->timerCb(this->timerCbUserData);
    }
  }

  void watchFd(int fd, void (*callback)(int, void*), void* userData) {
    this->fdCallbacks[fd] = std::pair(callback, userData);
  }
  void removeFd(int fd) {
    this->fdCallbacks.erase(fd);
  }
  void loopIter() {
    int i = 0;
    for (const auto& [fd, value] : this->fdCallbacks) {
      this->pollFds[i].fd = fd;
      this->pollFds[i].events = POLLIN;
      i++;
    }
    int len = i;
    poll(this->pollFds, len, -1);
    for (i = 0; i < len; i++) {
      if (this->pollFds[i].revents & POLLIN) {
        int fd = this->pollFds[i].fd;
        auto [cb, data] = this->fdCallbacks[fd];
        cb(fd, data);
      }
    }
  }

  void cleanup() {
    close(this->timerfd);
  }
  
};

static void timer_callback(int fd, void* data) {
  auto platform = ((LinuxPlatform*) data);
  platform->uponTimer();
}
#endif
