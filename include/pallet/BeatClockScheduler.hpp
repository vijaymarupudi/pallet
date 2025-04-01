#pragma once

#include "pallet/Clock.hpp"
#include <cstdint>
#include <type_traits>
#include <vector>
#include "pallet/containers/KeyedPriorityQueue.hpp"
#include "pallet/containers/StaticVector.hpp"
#include "pallet/containers/IdTable.hpp"
#include "pallet/constants.hpp"

namespace pallet {

using BeatClockSchedulerIdT = std::conditional_t<pallet::constants::isEmbeddedDevice,
                              uint8_t, uint32_t>;

struct BeatClockEventInfo {
  BeatClockSchedulerIdT id;
  double now;
  double intended;
  double period;
};


class BeatClockSchedulerInformationInterface {
public:
  virtual double getCurrentBeat() = 0;
  virtual double getCurrentBeatPeriod() = 0;
  virtual int getCurrentPPQN() = 0;
};

using BeatClockCbT = void(*)(BeatClockEventInfo*, void*);

class BeatClockScheduler {
public:
  using Id = BeatClockSchedulerIdT;
private:
  struct BeatClockEvent {
    double prev;
    double period;
    BeatClockCbT callback;
    void* callbackUserData;
    bool deleted;
    bool isInterval() { return period != 0; }
  };

  template <class T>
  using ContainerType = std::conditional_t<pallet::constants::isEmbeddedDevice,
                                           pallet::containers::StaticVector<T, 256>,
                                           std::vector<T>>;
  containers::KeyedPriorityQueue<double, Id, ContainerType, std::greater<double>> queue;
  containers::IdTable<BeatClockEvent, ContainerType, Id> idTable;
  BeatClockSchedulerInformationInterface* beatInfo;
public:
  bool clockTimeoutStatus = false;
  Clock::Id clockTimeoutId;
  Clock* clock;
  // public
  void init(Clock* clock, BeatClockSchedulerInformationInterface* beatInfo);
  void setBeatInfo(BeatClockSchedulerInformationInterface* beatInfo) { this->beatInfo = beatInfo; }
  Id setBeatTimeout(double duration,
                         BeatClockCbT callback,
                         void* callbackUserData);
  Id setBeatSyncTimeout(double sync,
                             double offset,
                             BeatClockCbT callback,
                             void* callbackUserData);
  Id setBeatTimeoutAbsolute(double goal,
                                 BeatClockCbT callback,
                                 void* callbackUserData);
  Id setBeatInterval(double period,
                          BeatClockCbT callback,
                          void* callbackUserData);
  Id setBeatSyncInterval(double sync,
                              double offset,
                              double period,
                              BeatClockCbT callback,
                              void* callbackUserData); 
  Id setBeatIntervalAbsolute(double goal,
                                  double period,
                                  BeatClockCbT callback,
                                  void* callbackUserData);
  void clearBeatTimeout(Id id);
  void* getBeatTimeoutUserData(Id id);
  void clearBeatSyncTimeout(Id id);
  void clearBeatInterval(Id id);
  double getCurrentBeat() {
    return beatInfo->getCurrentBeat();
  }

  // private
  void processEvent(BeatClockScheduler::Id id, double now, double goal);
  void updateWaitingTime();
  void process();
  void timer(pallet::Time time, bool off = false);
  void processSoon();
  pallet::Time targetBeatTime(double, double);
  void uponTick();
};

}
