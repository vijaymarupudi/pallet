#pragma once
#include <cstdint>
#include <type_traits>
#include <vector>
#include "pallet/Platform.hpp"

#include "pallet/containers/KeyedPriorityQueue.hpp"
#include "pallet/containers/StaticVector.hpp"
#include "pallet/containers/IdTable.hpp"
#include "pallet/measurement.hpp"
#include "pallet/constants.hpp"
#include "pallet/time.hpp"
#include "pallet/error.hpp"

namespace pallet {

namespace detail {

struct ClockPrecisionTimingManager {
  static constexpr int eventProcessingFactor = 10;
  static constexpr int spinFactor = 2;
  static_assert(eventProcessingFactor > spinFactor);
  pallet::RunningMeanMeasurer<double, 8> errorMeasurer;
  pallet::Time platformWaitTillTime = 0;

  pallet::Time tillWhenShouldPlatformWait(pallet::Time goalTime) {
    this->platformWaitTillTime = goalTime - errorMeasurer.mean() * spinFactor;
    return this->platformWaitTillTime;
  }

  bool shouldIProceedToEventProcessing(pallet::Time now, pallet::Time nextEventTime) {
    if (now >= nextEventTime) { return true; }
    auto duration = nextEventTime - now;
    return duration < (errorMeasurer.mean() * eventProcessingFactor);
  }

  // after this function, busy wait
  void beforeBusyWait(pallet::Time now) {
    auto err = now - this->platformWaitTillTime;

    // This is necessary to prevent large err values as a result of large io spikes

    // If those occur, it completely deadlocks the program since it
    // will busy wait for a LOT of time. In that case, it's better for
    // timing errors to happen.

    if (err < pallet::timeInUs(100)) {
      errorMeasurer.addSample(err);
    }
  }
};
}

using ClockIdT = std::conditional_t<pallet::constants::isEmbeddedDevice,
                              uint8_t, uint32_t>;

struct ClockEventInfo {
  ClockIdT id;
  pallet::Time now;
  pallet::Time intended;
  pallet::Time period;
  size_t overhead;
};

using ClockCbT = void(*)(ClockEventInfo*, void*);

class Clock {
public:
  using Id = ClockIdT;
private:
  struct ClockEvent {
    pallet::Time prev;
    pallet::Time period;
    ClockCbT callback;
    void* callbackUserData;
    bool deleted;
    bool isInterval() { return period != 0; }
  };

  template <class T>
  using ContainerType = std::conditional_t<pallet::constants::isEmbeddedDevice,
                                           pallet::containers::StaticVector<T, 256>,
                                           std::vector<T>>;

  containers::KeyedPriorityQueue<pallet::Time, Id, ContainerType, std::greater<pallet::Time>> queue;
  containers::IdTable<ClockEvent, ContainerType, Id> idTable;
  Platform& platform;
  bool platformTimerStatus = false;
  pallet::Time waitingTime = 0;
  detail::ClockPrecisionTimingManager precisionTimingManager;
public:
  static Result<Clock> create(Platform& platform);
  Clock(Platform& platform);
  pallet::Time currentTime();
  Id setTimeout(pallet::Time duration,
                     ClockCbT callback,
                     void* callbackUserData);
  Id setTimeoutAbsolute(pallet::Time goal,
                             ClockCbT callback,
                             void* callbackUserData);
  Id setInterval(pallet::Time period,
                      ClockCbT callback,
                      void* callbackUserData);
  Id setIntervalAbsolute(pallet::Time goal,
                              pallet::Time period,
                              ClockCbT callback,
                              void* callbackUserData);
  void clearTimeout(Id id);
  void* getTimeoutUserData(Id id);
  void clearInterval(Id id);
  void processEvent(Clock::Id id, pallet::Time goal);
  void updateWaitingTime();
  void process();

};

}
