#include "Platform.hpp"

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

#include <map>
#include <utility>
#include <poll.h>
#include <unistd.h>
#include <cstdio>
#include <sys/timerfd.h>
#include <time.h>

static void timeToTimespec(uint64_t time, struct timespec* spec) {
  time_t s = time / 1000000;
  time_t ns = (time % 1000000) * 1000;
  spec->tv_sec = s;
  spec->tv_nsec = ns;
}

static uint64_t timespecToTime(struct timespec* spec) {
  return (spec->tv_sec * 1000000) + (spec->tv_nsec / 1000);
}

static void timerCallback(int fd, void* data);

static void linuxSetThreadToHighPriority() {
  int ret = getpriority(PRIO_PROCESS, 0);
  // increase priority of the process
  setpriority(PRIO_PROCESS, 0, ret - 4);
}

void LinuxPlatform::init() {
  timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
  this->watchFdIn(timerfd, &timerCallback, this);

  // for reducing latency
  this->cpu_dma_latency_fd = open("/dev/cpu_dma_latency", O_RDONLY);
  if (this->cpu_dma_latency_fd >= 0) {
    int val = 0;
    write(this->cpu_dma_latency_fd, &val, sizeof(val));
  }

  // for reducing latency
  linuxSetThreadToHighPriority();
}

uint64_t LinuxPlatform::currentTime() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  return timespecToTime(&spec);
}

void LinuxPlatform::timer(uint64_t time) {
  // 0 to disarm
  struct timespec it_interval = {0};
  struct timespec it_value = {0};
  if (time != 0) {
    timeToTimespec(time, &it_value);  
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

void LinuxPlatform::watchFdIn(int fd, void (*callback)(int, void*), void* userData) {
  this->fdCallbacks[fd] = std::tuple(POLLIN, callback, userData);
}
void LinuxPlatform::removeFd(int fd) {
  this->fdCallbacks.erase(fd);
}
void LinuxPlatform::loopIter() {
  int i = 0;
  for (const auto& [fd, value] : this->fdCallbacks) {
    const auto& [events, cb, data] = value;
    this->pollFds[i].fd = fd;
    this->pollFds[i].events = events;
    i++;
  }
  int len = i;
  poll(this->pollFds, len, -1);
  for (i = 0; i < len; i++) {
    if (this->pollFds[i].revents & POLLIN) {
      int fd = this->pollFds[i].fd;
      auto& [events, cb, data] = this->fdCallbacks[fd];
      cb(fd, data);
    }
  }
}

void LinuxPlatform::setFdNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void LinuxPlatform::cleanup() {
  close(this->timerfd);
  if (this->cpu_dma_latency_fd >= 0) { close(this->cpu_dma_latency_fd); }
}

static void timerCallback(int fd, void* data) {
  auto platform = ((LinuxPlatform*) data);
  platform->uponTimer();
}
#endif
