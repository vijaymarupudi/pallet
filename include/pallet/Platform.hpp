#pragma once

#include "pallet/error.hpp"
#include "pallet/time.hpp"

namespace pallet {

class Platform {
  public:
  
  void setOnTimer(void (*cb)(void*), void* userData) {
    this->timerCb = cb;
    this->timerCbUserData = userData;
  }

  void busyWaitUntil(auto&& condFunc) {
    while (!condFunc()) {
      this->pause();
    }
  }

  // Required to be thread safe!
  virtual pallet::Time currentTime() = 0;
  virtual void timer(pallet::Time time, bool off = false) = 0;
  virtual void pause() = 0;
  virtual void quit() = 0;
  virtual ~Platform() {};

protected:
  
  void (*timerCb)(void*);
  void* timerCbUserData;
};

}
