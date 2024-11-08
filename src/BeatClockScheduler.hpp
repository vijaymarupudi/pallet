#pragma once

#include "Clock.hpp"
#include <inttypes.h>
#include <type_traits>
#include <vector>
#include "KeyedPriorityQueue.hpp"
#include "StaticVector.hpp"
#include "IdTable.hpp"
#include "constants.hpp"

using BeatClockSchedulerIdT = std::conditional_t<pallet::constants::isEmbeddedDevice,
                              uint8_t, uint32_t>;

struct BeatClockEventInfo {
  BeatClockSchedulerIdT id;
  double now;
  double intended;
  double period;
};


class BeatClockSchedulerInformationInterface {
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
                                           StaticVector<T, 256>,
                                           std::vector<T>>;
  KeyedPriorityQueue<double, id_type, ContainerType, std::greater<double>> queue;
  IdTable<BeatClockEvent, ContainerType, id_type> idTable;
  BeatClockSchedulerInformationInterface* beatInfo;
public:
  void init(BeatClockSchedulerInformationInterface* beatInfo);
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
  id_type setBeatIntervalAbsolute(double goal,
                                  double period,
                                  BeatClockCbT callback,
                                  void* callbackUserData);
  void clearBeatTimeout(id_type id);
  void* getBeatTimeoutUserData(id_type id);
  void clearBeatSyncTimeout(id_type id);
  void clearBeatInterval(id_type id);
  
private:
  void processEvent(BeatClockScheduler::id_type id, double now, double goal);
  void updateWaitingTime();
  void process();
};

