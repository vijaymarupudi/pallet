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
    errorMeasurer.addSample(err);
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
};

using ClockCbT = void(*)(ClockEventInfo*, void*);

class Clock {
public:
  using id_type = ClockIdT;
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

  containers::KeyedPriorityQueue<pallet::Time, id_type, ContainerType, std::greater<pallet::Time>> queue;
  containers::IdTable<ClockEvent, ContainerType, id_type> idTable;
  Platform& platform;
  bool platformTimerStatus = false;
  pallet::Time waitingTime = 0;
  detail::ClockPrecisionTimingManager precisionTimingManager;
public:
  static Result<Clock> create(Platform& platform);
  Clock(Platform& platform);
  pallet::Time currentTime();
  id_type setTimeout(pallet::Time duration,
                     ClockCbT callback,
                     void* callbackUserData);
  id_type setTimeoutAbsolute(pallet::Time goal,
                             ClockCbT callback,
                             void* callbackUserData);
  id_type setInterval(pallet::Time period,
                      ClockCbT callback,
                      void* callbackUserData);
  id_type setIntervalAbsolute(pallet::Time goal,
                              pallet::Time period,
                              ClockCbT callback,
                              void* callbackUserData);
  void clearTimeout(id_type id);
  void* getTimeoutUserData(id_type id);
  void clearInterval(id_type id);
  void processEvent(Clock::id_type id, pallet::Time goal);
  void updateWaitingTime();
  void process();

};

}
