#include "Platform.hpp"

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

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

void LinuxPlatform::init() {
  timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  fdCallbacks[timerfd] = std::pair(&timer_callback, this);
}

uint64_t LinuxPlatform::currentTime() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  return timespec_to_time(&spec);
}

void LinuxPlatform::timer(uint64_t time) {
  // 0 to disarm
  struct timespec it_interval = {0};
  struct timespec it_value = {0};
  if (time != 0) {
    time_to_timespec(time, &it_value);  
  }
  struct itimerspec its = {it_interval, it_value};
  timerfd_settime(this->timerfd, TFD_TIMER_ABSTIME, &its, nullptr);
}

void LinuxPlatform::uponTimer() {
  uint64_t expirations;
  read(this->timerfd, &expirations, sizeof(expirations));
  if (this->timerCb) {
    this->timerCb(this->timerCbUserData);
  }
}

void LinuxPlatform::watchFd(int fd, void (*callback)(int, void*), void* userData) {
  this->fdCallbacks[fd] = std::pair(callback, userData);
}
void LinuxPlatform::removeFd(int fd) {
  this->fdCallbacks.erase(fd);
}
void LinuxPlatform::loopIter() {
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

void LinuxPlatform::cleanup() {
  close(this->timerfd);
}

static void timer_callback(int fd, void* data) {
  auto platform = ((LinuxPlatform*) data);
  platform->uponTimer();
}
#endif
