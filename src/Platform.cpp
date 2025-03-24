#include "Platform.hpp"

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

#include <map>
#include <utility>
#include <cstdio>
#include <cstring>

#include <poll.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <time.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/resource.h>

#include "macros.hpp"


// Most likely buggy

// template <class UIntType>
// std::pair<UIntType, bool> unsignedAdditionHelper(UIntType a, UIntType b) {
//   bool overflow;
//   UIntType res = a + b;
//   if (res < a) { overflow = true; }
//   else { overflow = false; }
//   return {res, overflow};
// }

// template <class UIntType>
// std::pair<UIntType, bool> unsignedSubtractionHelper(UIntType a, UIntType b) {
//   bool underflow;
//   UIntType res = a - b;
//   if (res > a) {
//     underflow = true;

//   }
//   else { underflow = false; }
//   return {res, underflow};
// }

namespace pallet {

template <class UIntType>
std::pair<UIntType, bool> nanosecondAdditionHelper(UIntType a, UIntType b) {
  bool overflow;
  UIntType res = a + b;
  if (res > 1000000000) {
    overflow = true;
    res = res - 1000000000;
  }
  else { overflow = false; }
  return {res, overflow};
}

template <class UIntType>
std::pair<UIntType, bool> nanosecondSubtractionHelper(UIntType a, UIntType b) {
  bool underflow;
  UIntType res;
  if (b > a) {
    underflow = true;
    res = 1000000000 - (b - a);
  } else {
    underflow = false;
    res = a - b;
  }
  return {res, underflow};
}


static void timeToTimespec(struct timespec* reference, uint64_t time, struct timespec* spec) {
  auto sSince = time / 1000000000;
  auto nsSince = (time % 1000000000);
  time_t s = sSince + reference->tv_sec;
  auto [ns, overflow] = nanosecondAdditionHelper<uint64_t>(nsSince, reference->tv_nsec);
  if (overflow) { s += 1; }
  spec->tv_sec = s;
  spec->tv_nsec = ns;
}

static uint64_t timespecToTime(struct timespec* reference, struct timespec* spec) {
  auto [ns, underflow] = nanosecondSubtractionHelper<uint64_t>(spec->tv_nsec, reference->tv_nsec);
  time_t s = spec->tv_sec - reference->tv_sec;
  if (underflow) {
    s -= 1;
  }
  return (s * 1000000000) + ns;
}

static void timerCallback(int fd, int revents, void* data);

static void linuxSetThreadToHighPriority() {
  // increase the niceness of the process
  int ret = getpriority(PRIO_PROCESS, 0);
  setpriority(PRIO_PROCESS, 0, ret - 4);

  // enable SCHED_FIFO for realtime scheduling without preemption
  // new threads can't inherit realtime status
  struct sched_param schparam;
  memset(&schparam, 0, sizeof(struct sched_param));
  schparam.sched_priority = 60;
  if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK,
                         &schparam) == 0) {
    fprintf(stderr, "realtime scheduling enabled\n");
  }
}

Result<LinuxPlatform> LinuxPlatform::create() {
  return Result<LinuxPlatform>(std::in_place_t{});
}

LinuxPlatform::LinuxPlatform() {
  // get reference time
  clock_gettime(CLOCK_MONOTONIC, &referenceTime);

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
  auto t = timespecToTime(&this->referenceTime, &spec);
  return t;
}

void LinuxPlatform::timer(uint64_t time, bool off) {
  // if off = true, turn off
  struct timespec it_interval = {};
  struct timespec it_value = {};
  if (!off) {
    timeToTimespec(&this->referenceTime, time, &it_value);
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


LinuxPlatform::FdPollState& LinuxPlatform::findOrCreateFdPollState(int fd) {
  auto it = this->fdCallbacks.find(fd);
  if (it == this->fdCallbacks.end()) {
    return (this->fdCallbacks[fd] = FdPollState({0, nullptr, nullptr}));
  } else {
    return std::get<1>(*it);
  }
}

void LinuxPlatform::watchFdEvents(int fd, int events, FdCallback callback, void* userData) {
  auto& state = this->findOrCreateFdPollState(fd);
  state.events |= events;
  state.callback = callback;
  state.ud = userData;
}

void LinuxPlatform::watchFdIn(int fd, FdCallback callback, void* userData) {
  return watchFdEvents(fd, POLLIN, callback, userData);
}
void LinuxPlatform::watchFdOut(int fd, FdCallback callback, void* userData) {
  return watchFdEvents(fd, POLLOUT, callback, userData);
}

void LinuxPlatform::unwatchFdIn(int fd) {
  this->unwatchFdEvents(fd, POLLIN);
}

void LinuxPlatform::unwatchFdOut(int fd) {
  this->unwatchFdEvents(fd, POLLOUT);
}

void LinuxPlatform::unwatchFdEvents(int fd, int events) {
  auto it = this->fdCallbacks.find(fd);
  if (it != this->fdCallbacks.end()) {
    auto& [fd, state] = *it;
    state.events &= ~events;
    if (!state.events) {
      this->removeFd(fd);
    }
  }
}

void LinuxPlatform::removeFd(int fd) {
  this->fdCallbacks.erase(fd);
}
void LinuxPlatform::loopIter() {
  int i = 0;
  for (const auto& [fd, state] : this->fdCallbacks) {
    this->pollFds[i].fd = fd;
    this->pollFds[i].events = state.events;
    i++;
  }
  int len = i;
  poll(this->pollFds, len, -1);
  for (i = 0; i < len; i++) {
    if (this->pollFds[i].revents) {
      int fd = this->pollFds[i].fd;
      auto& state = this->fdCallbacks[fd];
      state.callback(fd, this->pollFds[i].revents, state.ud);
    }
  }
}

void LinuxPlatform::setFdNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


LinuxPlatform::~LinuxPlatform() {
  close(this->timerfd);
  if (this->cpu_dma_latency_fd >= 0) { close(this->cpu_dma_latency_fd); }
}

static void timerCallback(int fd, int revents, void* data) {
  (void)fd;
  (void)revents;
  auto platform = ((LinuxPlatform*) data);
  platform->uponTimer();
}

/*
 * FdManager
 */

void FdManager::uponWriteReady() {
  unsigned char* data = static_cast<unsigned char*>(writeState.data);
  ssize_t writtenCount = ::write(this->fd, data + writeState.writtenLen,
                                 writeState.len - writeState.writtenLen);
  writeState.writtenLen += writtenCount;
  if (writeState.writtenLen == writeState.len) {
    this->platform.unwatchFdOut(fd);
    writeState.cb(this->fd, writeState.ud);
  }
}

void FdManager::uponReadReady() {
  unsigned char buf[8192];
  ssize_t count = read(this->fd, buf, 8192);
  readState.cb(this->fd, buf, (size_t)count, readState.ud);
}

void FdManager::uponReady(int revents) {
  if (revents & LinuxPlatform::Read) {
    this->uponReadReady();
  }

  if (revents & LinuxPlatform::Write) {
    this->uponWriteReady();
  }
}

FdManager::FdManager(LinuxPlatform& platform, int fd) : platform(platform), fd(fd) {}

void FdManager::setFd(int fd) { this->fd = fd; }
  
void FdManager::write(void* data, size_t len, WriteCallback cb, void* ud) {
  writeState = {data, len, 0, cb, ud};
  this->platform.watchFdOut(this->fd, FdManager::platformCallback, this);
}

void FdManager::startReading(ReadCallback cb, void* ud) {
  readState = {cb, ud};
  this->platform.watchFdIn(fd, FdManager::platformCallback, this);
}

void FdManager::stopReading() {
  this->platform.unwatchFdIn(this->fd);
}

FdManager::~FdManager() {
  if (fd >= 0) {
    this->platform.unwatchFdEvents(this->fd, LinuxPlatform::Read | LinuxPlatform::Write);
  }
}

void FdManager::platformCallback(int fd, int revents, void* ud) {
  (void)fd;
  auto thi = (FdManager*)ud;
  thi->uponReady(revents);
}

}
#endif
