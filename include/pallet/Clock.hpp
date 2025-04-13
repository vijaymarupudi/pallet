#pragma once

#include <type_traits>
#include <vector>

#include "pallet/ClockInterface.hpp"
#include "pallet/Platform.hpp"
#include "pallet/containers/KeyedPriorityQueue.hpp"
#include "pallet/containers/StaticVector.hpp"
#include "pallet/containers/IdTable.hpp"
#include "pallet/measurement.hpp"
#include "pallet/constants.hpp"
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

class Clock final : public ClockInterface {
private:
  struct ClockEvent {
    pallet::Time prev;
    pallet::Time period;
    Callback callback;
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
  virtual pallet::Time currentTime() override;
  virtual Id setTimeout(pallet::Time duration,
                Callback callback) override;
  virtual Id setTimeoutAbsolute(pallet::Time goal,
                        Callback callback) override;
  virtual Id setInterval(pallet::Time period,
                 Callback callback) override;
  virtual Id setIntervalAbsolute(pallet::Time goal,
                         pallet::Time period,
                         Callback callback) override;
  virtual void clearTimeout(Id id) override;
  virtual void clearInterval(Id id) override;


  void processEvent(Clock::Id id, pallet::Time goal);
  void updateWaitingTime();
  void process();

};

}
