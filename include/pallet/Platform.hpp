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
  // Required to be thread safe!
  virtual pallet::Time currentTime() = 0;
  virtual void timer(pallet::Time time, bool off = false) = 0;

protected:
  
  void (*timerCb)(void*);
  void* timerCbUserData;
};

}
