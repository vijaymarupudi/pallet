#pragma once
#include <inttypes.h>

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
#include <utility>
#include <poll.h>
#include <time.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <unistd.h>

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
  struct pollfd pollFds[16];
  std::map<int, FdPollState> fdCallbacks;
  void uponTimer();

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

static void fdManagerPlatformCallback(int fd, int revents, void* ud);

class FdManager {

  friend void fdManagerPlatformCallback(int fd, int revents, void* ud);
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


  void uponWriteReady() {
    unsigned char* data = reinterpret_cast<unsigned char*>(writeState.data);
    ssize_t writtenCount = ::write(this->fd, data + writeState.writtenLen,
                                   writeState.len - writeState.writtenLen);
    writeState.writtenLen += writtenCount;
    if (writeState.writtenLen == writeState.len) {
      this->platform.unwatchFdOut(fd);
      writeState.cb(this->fd, writeState.ud);
    }
  }

  void uponReadReady() {
    unsigned char buf[8192];
    ssize_t count = read(this->fd, buf, 8192);
    readState.cb(this->fd, buf, (size_t)count, readState.ud);
  }

  void uponReady(int revents) {
    if (revents & LinuxPlatform::Read) {
      this->uponReadReady();
    }

    if (revents & LinuxPlatform::Write) {
      this->uponWriteReady();
    }
  }

public:

  FdManager(LinuxPlatform& platform, int fd = -1) : platform(platform), fd(fd) {}

  void setFd(int fd) { this->fd = fd; }
  
  void write(void* data, size_t len, WriteCallback cb, void* ud) {
    writeState = {data, len, 0, cb, ud};
    this->platform.watchFdOut(this->fd, fdManagerPlatformCallback, this);
  }

  void startReading(ReadCallback cb, void* ud) {
    readState = {cb, ud};
    this->platform.watchFdIn(fd, fdManagerPlatformCallback, this);
  }

  void stopReading() {
    this->platform.unwatchFdIn(this->fd);
  }

  ~FdManager() {
    if (fd >= 0) {
      this->platform.unwatchFdEvents(this->fd, LinuxPlatform::Read | LinuxPlatform::Write);
    }
  }
};

static void fdManagerPlatformCallback(int fd, int revents, void* ud) {
  (void)fd;
  auto thi = (FdManager*)ud;
  thi->uponReady(revents);
}

#endif

}
