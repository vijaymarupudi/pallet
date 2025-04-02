#include "pallet/Clock.hpp"
#include "pallet/measurement.hpp"

namespace pallet {

static void clock_timer_callback(void* data);

Clock::Clock(Platform& platform) : platform(platform) {
  this->platform.setOnTimer(clock_timer_callback, this);
}

pallet::Time Clock::currentTime() {
  return platform.currentTime();
}

Clock::Id Clock::setTimeout(pallet::Time duration,
                                 ClockCbT callback,
                                 void* callbackUserData) {
  auto now = this->currentTime();
  auto goal = now + duration;
  return setTimeoutAbsolute(goal, callback, callbackUserData);
}

Clock::Id Clock::setTimeoutAbsolute(pallet::Time goal,
                                         ClockCbT callback,
                                         void* callbackUserData) {
  auto id = idTable.push(ClockEvent {
      0, 0, callback, callbackUserData, false
    });
  queue.push(goal, id);
  this->updateWaitingTime();
  return id;
}

Clock::Id Clock::setInterval(pallet::Time period,
                                  ClockCbT callback,
                                  void* callbackUserData){
  auto now = this->currentTime();
  return setIntervalAbsolute(now + period, period, callback, callbackUserData);
}

Clock::Id Clock::setIntervalAbsolute(pallet::Time goal,
                                          pallet::Time period,
                                          ClockCbT callback,
                                          void* callbackUserData){
  auto id = idTable.push(ClockEvent {
      goal - period, period, callback, callbackUserData, false
    });
  queue.push(goal, id);
  this->updateWaitingTime();
  return id;
}

void Clock::clearTimeout(Clock::Id id) {
  idTable[id].deleted = true;
}

void* Clock::getTimeoutUserData(Clock::Id id) {
  return idTable[id].callbackUserData;
}

void Clock::clearInterval(Clock::Id id) {
  this->clearTimeout(id);
}

void Clock::processEvent(Clock::Id id, pallet::Time goal) {
  // at this point, the event is out of the queue, but still in the
  // id table
  ClockEvent* event = &idTable[id];

  // callback and reschedule if needed
  if (!event->deleted) {

    // callbacks should never be called before the asked for time has happened
    // use the precisionTimingManager to update measurements and then busyWait
    auto now = this->currentTime();
    precisionTimingManager.beforeBusyWait(now);
    size_t overhead = 0;
    platform.busyWaitUntil([&]() {
      overhead += 1;
      now = this->currentTime();
      return now > goal;
    });

    ClockEventInfo info {id, now, goal, event->period, overhead};
    event->callback(&info, event->callbackUserData);
  }

  // This is necessary, as idTable might have reallocated in the
  // callback, and therefore the reference might be invalid
  event = &idTable[id];

  if (!event->deleted && event->period != 0) {
    // if interval, add it back to the queue
    auto now = event->prev + event->period;
    event->prev = now;
    queue.push(now + event->period, id);
    this->updateWaitingTime();
  } else {
    // we will never need this event again
    idTable.free(id);
  }
}

void Clock::updateWaitingTime() {
  if (queue.size() == 0) {
    this->waitingTime = 0;
    this->platformTimerStatus = false;
    this->platform.timer(0, true);
    return;
  }
  auto [ttime, tevent] = this->queue.top();
  if (this->platformTimerStatus && this->waitingTime == ttime) { return; }
  else {
    this->waitingTime = ttime;
    auto platformWaitTime = precisionTimingManager.tillWhenShouldPlatformWait(ttime);
    this->platform.timer(platformWaitTime);
    this->platformTimerStatus = true;
  }
}

void Clock::process() {
  while (true) {
    if (queue.size() == 0) { break; }
    auto now = this->currentTime();
    auto [ttime, teventid] = queue.top();
    if (precisionTimingManager.shouldIProceedToEventProcessing(now, ttime) ||
        idTable[teventid].deleted) {
      // run the event!
      auto [time, eventid] = queue.top();
      queue.pop();
      processEvent(eventid, time);
    } else {
      break;
    }
  }
  this->updateWaitingTime();
}

static void clock_timer_callback(void* data) {
  ((Clock*)data)->process();
}


Result<Clock> Clock::create(Platform& platform) {
  return Result<Clock>(std::in_place_t{}, platform);
}

}
