#pragma once

#include "Clock.hpp"
#include <inttypes.h>
#include <type_traits>
#include <vector>
#include "containers/KeyedPriorityQueue.hpp"
#include "containers/StaticVector.hpp"
#include "containers/IdTable.hpp"
#include "constants.hpp"

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
  using id_type = BeatClockSchedulerIdT;
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
  containers::KeyedPriorityQueue<double, id_type, ContainerType, std::greater<double>> queue;
  containers::IdTable<BeatClockEvent, ContainerType, id_type> idTable;
  BeatClockSchedulerInformationInterface* beatInfo;
public:
  bool clockTimeoutStatus = false;
  Clock::id_type clockTimeoutId;
  Clock* clock;
  // public
  void init(Clock* clock, BeatClockSchedulerInformationInterface* beatInfo);
  void setBeatInfo(BeatClockSchedulerInformationInterface* beatInfo) { this->beatInfo = beatInfo; }
  id_type setBeatTimeout(double duration,
                         BeatClockCbT callback,
                         void* callbackUserData);
  id_type setBeatSyncTimeout(double sync,
                             double offset,
                             BeatClockCbT callback,
                             void* callbackUserData);
  id_type setBeatTimeoutAbsolute(double goal,
                                 BeatClockCbT callback,
                                 void* callbackUserData);
  id_type setBeatInterval(double period,
                          BeatClockCbT callback,
                          void* callbackUserData);
  id_type setBeatSyncInterval(double sync,
                              double offset,
                              double period,
                              BeatClockCbT callback,
                              void* callbackUserData); 
  id_type setBeatIntervalAbsolute(double goal,
                                  double period,
                                  BeatClockCbT callback,
                                  void* callbackUserData);
  void clearBeatTimeout(id_type id);
  void* getBeatTimeoutUserData(id_type id);
  void clearBeatSyncTimeout(id_type id);
  void clearBeatInterval(id_type id);
  double getCurrentBeat() {
    return beatInfo->getCurrentBeat();
  }

  // private
  void processEvent(BeatClockScheduler::id_type id, double now, double goal);
  void updateWaitingTime();
  void process();
  void timer(pallet::Time time, bool off = false);
  void processSoon();
  pallet::Time targetBeatTime(double, double);
  void uponTick();
};

}
