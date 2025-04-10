#include "pallet/constants.hpp"

#ifdef PALLET_CONSTANTS_PLATFORM_IS_POSIX

#include "pallet/PosixPlatform.hpp"
#include "pallet/posix.hpp"

#include <map>
#include <utility>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <poll.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>

// not posix
#include <sys/timerfd.h>

#include "pallet/macros.hpp"

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


static void timeToTimespec(struct timespec* reference, pallet::Time time, struct timespec* spec) {
  auto sSince = time / 1000000000;
  auto nsSince = (time % 1000000000);
  time_t s = sSince + reference->tv_sec;
  auto [ns, overflow] = nanosecondAdditionHelper<uint64_t>(nsSince, reference->tv_nsec);
  if (overflow) { s += 1; }
  spec->tv_sec = s;
  spec->tv_nsec = ns;
}

static pallet::Time timespecToTime(struct timespec* reference, struct timespec* spec) {
  auto [ns, underflow] = nanosecondSubtractionHelper<uint64_t>(spec->tv_nsec, reference->tv_nsec);
  time_t s = spec->tv_sec - reference->tv_sec;
  if (underflow) {
    s -= 1;
  }
  return (s * 1000000000) + ns;
}

Result<PosixPlatform> PosixPlatform::create() {
  return Result<PosixPlatform>(std::in_place_t{});
}

PosixPlatform::PosixPlatform() {
  // get reference time
  clock_gettime(CLOCK_MONOTONIC, &referenceTime);

  timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);

  auto timerCallback = [](int fd, short revents, void* data) {
    (void)fd;
    (void)revents;
    auto platform = static_cast<PosixPlatform*>(data);
    platform->uponTimer();
  };
  
  this->watchFdIn(timerfd, timerCallback, this);
}

PosixPlatform::~PosixPlatform() {
  this->unwatchFdEvents(timerfd, POLLIN);
  close(this->timerfd);
}

pallet::Time PosixPlatform::currentTime() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  auto t = timespecToTime(&this->referenceTime, &spec);
  return t;
}

void PosixPlatform::timer(pallet::Time time, bool off) {
  // if off = true, turn off
  struct timespec it_interval = {};
  struct timespec it_value = {};

  
  // if off then the structs will be zero
  if (!off) {
    this->timerGoalTime = time;
    timeToTimespec(&this->referenceTime, this->timerGoalTime, &it_value);
  }

  struct itimerspec its = {it_interval, it_value};
  timerfd_settime(this->timerfd, TFD_TIMER_ABSTIME, &its, nullptr);
}

void PosixPlatform::uponTimer() {
  uint64_t expirations;
  read(this->timerfd, &expirations, sizeof(expirations));
  
  if (this->timerCb) {
    this->timerCb(this->timerCbUserData);
  }
}


PosixPlatform::FdPollState& PosixPlatform::findOrCreateFdPollState(int fd) {
  auto it = this->fdCallbacks.find(fd);
  if (it == this->fdCallbacks.end()) {
    return (this->fdCallbacks[fd] = FdPollState({fd, 0, nullptr, nullptr}));
  } else {
    return std::get<1>(*it);
  }
}

void PosixPlatform::watchFdEvents(int fd, short events, FdCallback callback, void* userData) {
  auto& state = this->findOrCreateFdPollState(fd);
  state.events |= events;
  state.callback = callback;
  state.ud = userData;
}

void PosixPlatform::watchFdIn(int fd, FdCallback callback, void* userData) {
  return watchFdEvents(fd, POLLIN, callback, userData);
}
void PosixPlatform::watchFdOut(int fd, FdCallback callback, void* userData) {
  return watchFdEvents(fd, POLLOUT, callback, userData);
}

void PosixPlatform::unwatchFdIn(int fd) {
  this->unwatchFdEvents(fd, POLLIN);
}

void PosixPlatform::unwatchFdOut(int fd) {
  this->unwatchFdEvents(fd, POLLOUT);
}

void PosixPlatform::unwatchFdEvents(int fd, short events) {
  auto it = this->fdCallbacks.find(fd);
  if (it != this->fdCallbacks.end()) {
    auto& [fd, state] = *it;
    state.events &= ~events;
    if (!state.events) {
      this->removeFd(fd);
    }
  }
}

void PosixPlatform::removeFd(int fd) {
  this->fdCallbacks.erase(fd);
}
void PosixPlatform::loopIter() {
  this->pollFds.clear();
  for (const auto& [fd, state] : this->fdCallbacks) {
    struct pollfd item = {};
    item.fd = fd;
    item.events = state.events;
    this->pollFds.push_back(std::move(item));
  }
  poll(this->pollFds.data(), this->pollFds.size(), -1);
  for (size_t i = 0; i < this->pollFds.size(); i++) {
    if (this->pollFds[i].revents) {
      int fd = this->pollFds[i].fd;
      assert(this->fdCallbacks.find(fd) != this->fdCallbacks.end());
      auto& state = this->fdCallbacks[fd];
      state.callback(fd, this->pollFds[i].revents, state.ud);
    }
  }
}

void PosixPlatform::loop() {
  while (this->mRunning) {
    this->loopIter();
  }
}

void PosixPlatform::quit() {
  this->mRunning = false;
}

void PosixPlatform::setFdNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void PosixPlatform::pause() {
  pallet::architecturePause();
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
    this->platform->unwatchFdOut(fd);
    writeState.cb(this->fd, writeState.ud);
  }
}

void FdManager::uponReadReady() {
  unsigned char buf[8192];
  ssize_t count = read(this->fd, buf, 8192);
  readState.cb(this->fd, buf, (size_t)count, readState.ud);
}

void FdManager::uponReady(short revents) {
  if (revents & PosixPlatform::Read) {
    this->uponReadReady();
  }

  if (revents & PosixPlatform::Write) {
    this->uponWriteReady();
  }
}

FdManager::FdManager(PosixPlatform& platform, int fd) : platform(&platform), fd(fd) {}

FdManager::FdManager(FdManager&& other)
  : platform{std::move(other.platform)},
    writeState{std::move(other.writeState)},
    readState{std::move(other.readState)},
    fd{other.fd},
    revents{other.revents}
{
  // unwatch previous, because the this pointer would refer to dead memory
  auto events = other.revents;
  other.stopAll();
  other.fd = -1;
  // watch the current one
  this->rewatch(events);
}

FdManager& FdManager::operator=(FdManager&& other) {
  auto oevents = other.revents;
  auto events = this->revents;

  this->stopAll();
  other.stopAll();

  std::swap(platform, other.platform);
  std::swap(writeState, other.writeState);
  std::swap(readState, other.readState);
  std::swap(fd, other.fd);
  std::swap(revents, other.revents);

  this->rewatch(oevents);
  other.rewatch(events);
  return *this;
}

void FdManager::setFd(int fd) { this->fd = fd; }

void FdManager::write(void* data, size_t len, WriteCallback cb, void* ud) {
  this->revents |= PosixPlatform::Write;
  writeState = {data, len, 0, cb, ud};
  this->platform->watchFdEvents(this->fd, this->revents,
                               FdManager::platformCallback, this);
}

void FdManager::startReading(ReadCallback cb, void* ud) {
  this->revents |= PosixPlatform::Read;
  readState = {cb, ud};
  this->platform->watchFdEvents(fd, this->revents, FdManager::platformCallback, this);
}

void FdManager::stopReading() {
  this->revents &= ~PosixPlatform::Read;
  this->platform->unwatchFdEvents(this->fd, PosixPlatform::Read);
}

void FdManager::stopWriting() {
  this->revents &= ~PosixPlatform::Write;
  this->platform->unwatchFdEvents(this->fd, PosixPlatform::Write);
}

FdManager::~FdManager() {
  if (fd >= 0) {
    this->platform->unwatchFdEvents(this->fd, this->revents);
  }
}

void FdManager::platformCallback(int fd, short revents, void* ud) {
  (void)fd;
  auto thi = static_cast<FdManager*>(ud);
  thi->uponReady(revents);
}

void FdManager::stopAll() {
  this->platform->unwatchFdEvents(this->fd, this->revents);
  this->revents = 0;
}

void FdManager::rewatch(short revents) {
  if (revents) {
    this->revents = revents;
    this->platform->watchFdEvents(fd, revents,
                                 FdManager::platformCallback, this);
  }
}

void FdManager::setReadWriteUserData(void* readUd, void* writeUd) {
  if (readUd) {
    this->readState.ud = readUd;
  }

  if (writeUd) {
    this->writeState.ud = writeUd;
  }
}

}
#endif
