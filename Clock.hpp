#pragma once
#include <inttypes.h>
#include "Platform.hpp"
#include "KeyedPriorityQueue.hpp"
#include "IdTable.hpp"

static void clock_timer_callback(void* data);

class Clock {

  struct ClockEvent {
    uint64_t prev;
    uint64_t period;
    void (*callback)(void*);
    void* callbackUserData;
    bool deleted;
  };

#ifdef RASPBERRYPI_PICO
  using id_type = uint8_t;
#else
  using id_type = uint32_t;
#endif
  KeyedPriorityQueue<256, uint64_t, id_type, std::greater<uint64_t>> queue;
  IdTable<ClockEvent, 256, id_type> idTable;
  Platform* platform;
  uint64_t waitingTime = 0;
public:
  void init(Platform* platform) {
    this->platform = platform;
    this->platform->setOnTimer(clock_timer_callback, this);
  }

  uint64_t currentTime() {
    return platform->currentTime();
  }

  id_type setTimeout(uint64_t duration,
                     void (*callback)(void*),
                     void* callbackUserData) {
    auto now = this->currentTime();
    auto id = idTable.push(ClockEvent {
        now, 0, callback, callbackUserData, false
      });
    queue.push(now + duration, id);
    this->updateWaitingTime();
    return id;
  }

  id_type setInterval(uint64_t period,
                      void (*callback)(void*),
                      void* callbackUserData) {
    auto now = this->currentTime();
    auto id = idTable.push(ClockEvent {
        now, period, callback, callbackUserData, false
      });
    queue.push(now + period, id);
    this->updateWaitingTime();
    return id;
  }

  void clearTimeout(id_type id) {
    idTable[id].deleted = true;
  }

  void* getTimeoutUserData(id_type id) {
    return idTable[id].callbackUserData;
  }

  void clearInterval(id_type id) {
    this->clearTimeout(id);
  }

  void processEvent(id_type id) {
    // at this point, the event is out of the queue, but still in the
    // id table
    ClockEvent& event = idTable[id];
    // callback and reschedule if needed
    
    if (!event.deleted) {
      event.callback(event.callbackUserData);
    }
    
    if (!event.deleted && event.period != 0) {
      // if interval, add it back to the queue
      event.prev = event.prev + event.period;
      queue.push(event.prev + event.period, id);
    } else {
      // we will never need this event again
      idTable.free(id);
    }
  }

  void updateWaitingTime() {
    if (queue.size() == 0) {
      waitingTime = 0;
      platform->timer(0);
      return;
    }
    auto [ttime, tevent] = queue.top();
    if (waitingTime == ttime) { return; }
    else {
      waitingTime = ttime;
      platform->timer(waitingTime);
    }
  }

  void process() {
    auto now = this->currentTime();
    while (true) {
      if (queue.size() == 0) { break; }
      auto [ttime, teventid] = queue.top();
      if (now < ttime) {
        break;
      }
      auto [time, eventid] = queue.pop();
      processEvent(eventid);
    }
    this->updateWaitingTime();
  }
};

static void clock_timer_callback(void* data) {
  ((Clock*)data)->process();
}
