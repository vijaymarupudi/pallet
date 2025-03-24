#pragma once
#include <inttypes.h>
#include <type_traits>
#include <vector>
#include "Platform.hpp"

#include "containers/KeyedPriorityQueue.hpp"
#include "containers/StaticVector.hpp"
#include "containers/IdTable.hpp"
#include "constants.hpp"
#include "time.hpp"
#include "error.hpp"

namespace pallet {

using ClockIdT = std::conditional_t<pallet::constants::isEmbeddedDevice,
                              uint8_t, uint32_t>;

struct ClockEventInfo {
  ClockIdT id;
  uint64_t now;
  uint64_t intended;
  uint64_t period;
};

using ClockCbT = void(*)(ClockEventInfo*, void*);

class Clock {
public:
  using id_type = ClockIdT;
private:
  struct ClockEvent {
    uint64_t prev;
    uint64_t period;
    ClockCbT callback;
    void* callbackUserData;
    bool deleted;
    bool isInterval() { return period != 0; }
  };

  template <class T>
  using ContainerType = std::conditional_t<pallet::constants::isEmbeddedDevice,
                                           pallet::containers::StaticVector<T, 256>,
                                           std::vector<T>>;

  containers::KeyedPriorityQueue<uint64_t, id_type, ContainerType, std::greater<uint64_t>> queue;
  containers::IdTable<ClockEvent, ContainerType, id_type> idTable;
  Platform& platform;
  bool platformTimerStatus = false;
  uint64_t waitingTime = 0;
public:
  static Result<Clock> create(Platform& platform);
  Clock(Platform& platform);
  uint64_t currentTime();
  id_type setTimeout(uint64_t duration,
                     ClockCbT callback,
                     void* callbackUserData);
  id_type setTimeoutAbsolute(uint64_t goal,
                             ClockCbT callback,
                             void* callbackUserData);
  id_type setInterval(uint64_t period,
                      ClockCbT callback,
                      void* callbackUserData);
  id_type setIntervalAbsolute(uint64_t goal,
                              uint64_t period,
                              ClockCbT callback,
                              void* callbackUserData);
  void clearTimeout(id_type id);
  void* getTimeoutUserData(id_type id);
  void clearInterval(id_type id);
  void processEvent(Clock::id_type id, uint64_t now, uint64_t goal);
  void updateWaitingTime();
  void process();
  
};

}
