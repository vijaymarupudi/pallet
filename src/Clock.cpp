#include "Clock.hpp"

namespace pallet {

static void clock_timer_callback(void* data);

void Clock::init(Platform* platform) {
  this->platform = platform;
  this->platform->setOnTimer(clock_timer_callback, this);
}

uint64_t Clock::currentTime() {
  return platform->currentTime();
}

Clock::id_type Clock::setTimeout(uint64_t duration,
                                 ClockCbT callback,
                                 void* callbackUserData) {
  auto now = this->currentTime();
  auto goal = now + duration;
  return setTimeoutAbsolute(goal, callback, callbackUserData);
}

Clock::id_type Clock::setTimeoutAbsolute(uint64_t goal,
                                         ClockCbT callback,
                                         void* callbackUserData) {
  auto id = idTable.push(ClockEvent {
      0, 0, callback, callbackUserData, false
    });
  queue.push(goal, id);
  this->updateWaitingTime();
  return id;
}

Clock::id_type Clock::setInterval(uint64_t period,
                                  ClockCbT callback,
                                  void* callbackUserData){
  auto now = this->currentTime();
  return setIntervalAbsolute(now + period, period, callback, callbackUserData);
}

Clock::id_type Clock::setIntervalAbsolute(uint64_t goal,
                                          uint64_t period,
                                          ClockCbT callback,
                                          void* callbackUserData){
  auto id = idTable.push(ClockEvent {
      goal - period, period, callback, callbackUserData, false
    });
  queue.push(goal, id);
  this->updateWaitingTime();
  return id;
}

void Clock::clearTimeout(Clock::id_type id) {
  idTable[id].deleted = true;
}

void* Clock::getTimeoutUserData(Clock::id_type id) {
  return idTable[id].callbackUserData;
}

void Clock::clearInterval(Clock::id_type id) {
  this->clearTimeout(id);
}

void Clock::processEvent(Clock::id_type id, uint64_t now, uint64_t goal) {
  // at this point, the event is out of the queue, but still in the
  // id table
  ClockEvent& event = idTable[id];
  // callback and reschedule if needed

  if (!event.deleted) {
    ClockEventInfo info {id, now, goal, event.period};
    event.callback(&info, event.callbackUserData);
  }

  // This is necessary, as idTable might have reallocated in the
  // callback, and therefore the reference might be invalid
  event = idTable[id];

  if (!event.deleted && event.period != 0) {
    // if interval, add it back to the queue
    auto now = event.prev + event.period;
    event.prev = now;
    queue.push(now + event.period, id);
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
    this->platform->timer(0, true);
    return;
  }
  auto [ttime, tevent] = this->queue.top();
  if (this->platformTimerStatus && this->waitingTime == ttime) { return; }
  else {
    this->waitingTime = ttime;
    this->platform->timer(waitingTime);
    this->platformTimerStatus = true;
  }
}

void Clock::process() {
  auto now = this->currentTime();
  while (true) {
    if (queue.size() == 0) { break; }
    auto [ttime, teventid] = queue.top();
    if (now < ttime) {
      break;
    }
    auto [time, eventid] = queue.top();
    queue.pop();
    processEvent(eventid, now, time);
  }
  this->updateWaitingTime();
}

static void clock_timer_callback(void* data) {
  ((Clock*)data)->process();
}

}
