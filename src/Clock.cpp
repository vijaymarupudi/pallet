#include "Clock.hpp"

static void clock_timer_callback(void* data);

void Clock::init(Platform* platform) {
  this->platform = platform;
  this->platform->setOnTimer(clock_timer_callback, this);
}

uint64_t Clock::currentTime() {
  return platform->currentTime();
}

Clock::id_type Clock::setTimeout(uint64_t duration,
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

Clock::id_type Clock::setInterval(uint64_t period,
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

void Clock::clearTimeout(Clock::id_type id) {
  idTable[id].deleted = true;
}

void* Clock::getTimeoutUserData(Clock::id_type id) {
  return idTable[id].callbackUserData;
}

void Clock::clearInterval(Clock::id_type id) {
  this->clearTimeout(id);
}

void Clock::processEvent(Clock::id_type id) {
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

void Clock::updateWaitingTime() {
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
    processEvent(eventid);
  }
  this->updateWaitingTime();
}

static void clock_timer_callback(void* data) {
  ((Clock*)data)->process();
}
