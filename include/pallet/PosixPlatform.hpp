#pragma once

#include <map>
#include <vector>

#include <poll.h>
#include <time.h>

#include "pallet/Platform.hpp"

#include "pallet/error.hpp"
#include "pallet/time.hpp"

namespace pallet {

class PosixPlatform : public Platform {
public:
  using FdCallback = void(*)(int fd, short revents, void* ud);

  static constexpr short Read  = POLLIN;
  static constexpr short Write = POLLOUT;

  static Result<PosixPlatform> create();
  PosixPlatform();
  ~PosixPlatform() override;
  void watchFdIn(int fd, FdCallback callback, void* userData);
  void watchFdOut(int fd, FdCallback callback, void* userData);
  void watchFdEvents(int fd, short events, FdCallback callback, void* userData);
  void unwatchFdIn(int fd);
  void unwatchFdOut(int fd);
  void unwatchFdEvents(int fd, short events);
  void removeFd(int fd);
  void loop();
  void cleanup();
  void setFdNonBlocking(int fd);

  virtual pallet::Time currentTime() override;
  virtual void timer(pallet::Time time, bool off = false) override;
  virtual void pause() override;
  virtual void quit() override;
private:

  struct FdPollState {
    int fd;
    short events;
    FdCallback callback;
    void* ud;
  };

  struct timespec referenceTime;
  std::vector<struct pollfd> pollFds;
  std::map<int, FdPollState> fdCallbacks;
  pallet::Time timerGoalTime = 0;

  // TODO: Not Posix
  int timerfd;


  void uponTimer();
  FdPollState& findOrCreateFdPollState(int fd);
  void loopIter();
  bool mRunning = true;
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

  PosixPlatform* platform;
  WriteState writeState;
  ReadState readState;
  int fd;
  short revents = 0;



  void uponWriteReady();
  void uponReadReady();
  void uponReady(short revents);
  void rewatch(short revents);

public:

  FdManager(PosixPlatform& platform, int fd = -1);
  FdManager(FdManager&& other);
  FdManager& operator=(FdManager&& other);

  void setFd(int fd);
  void write(void* data, size_t len, WriteCallback cb, void* ud);
  void setReadWriteUserData(void* = nullptr, void* = nullptr);
  void startReading(ReadCallback cb, void* ud);
  void stopReading();
  void stopWriting();
  void stopAll();
  ~FdManager();

  static void platformCallback(int fd, short revents, void* ud);
};

}
