#pragma once
#include <inttypes.h>
#include <type_traits>
#include <vector>
#include "Platform.hpp"
#include "KeyedPriorityQueue.hpp"
#include "StaticVector.hpp"
#include "IdTable.hpp"
#include "constants.hpp"

class Clock {

public:
  using id_type = std::conditional_t<pallet::constants::isEmbeddedDevice,
                                     uint8_t, uint32_t>;
private:

  struct ClockEvent {
    uint64_t prev;
    uint64_t period;
    void (*callback)(void*);
    void* callbackUserData;
    bool deleted;
  };

  template <class T>
  using ContainerType = std::conditional_t<pallet::constants::isEmbeddedDevice,
                                           StaticVector<T, 256>,
                                           std::vector<T>>;

  KeyedPriorityQueue<uint64_t, id_type, ContainerType, std::greater<uint64_t>> queue;
  IdTable<ClockEvent, ContainerType, id_type> idTable;
  Platform* platform;
  uint64_t waitingTime = 0;
public:
  void init(Platform* platform);
  uint64_t currentTime();
  id_type setTimeout(uint64_t duration,
                     void (*callback)(void*),
                     void* callbackUserData);
  id_type setInterval(uint64_t period,
                      void (*callback)(void*),
                      void* callbackUserData);
  void clearTimeout(id_type id);
  void* getTimeoutUserData(id_type id);
  void clearInterval(id_type id);
  void processEvent(id_type id);
  void updateWaitingTime();
  void process();
};
