#include "pallet/BeatClockScheduler.hpp"
#include <cmath>

namespace pallet {

static void beatClockSchedulerCallback(ClockEventInfo* info, void* ud) {
  (void)info;
  // This timer has expired
  static_cast<BeatClockScheduler*>(ud)->clockTimeoutStatus = false;
  static_cast<BeatClockScheduler*>(ud)->process();
}

static double beatClockSchedulerNextSyncedBeat(double clockBeat, double sync, double offset) {
    double nextBeat;
    nextBeat = ceil(clockBeat / sync + 0.000001) * sync;
    nextBeat = nextBeat + offset;

    while (nextBeat < clockBeat + 0.000001) {
        nextBeat += sync;
    }
    return fmax(nextBeat, 0);
}

void BeatClockScheduler::init(Clock* clock, BeatClockSchedulerInformationInterface* beatInfo) {
  this->clock = clock;
  this->beatInfo = beatInfo;
}

void BeatClockScheduler::timer(pallet::Time time, bool off) {

  if (clockTimeoutStatus) {
    clock->clearTimeout(clockTimeoutId);
    clockTimeoutStatus = false;
  }
  
  // turn off
  if (off) {
    return;
  }

  // others, absolute timeout
  clockTimeoutId = clock->setTimeoutAbsolute(time, beatClockSchedulerCallback, this);
  clockTimeoutStatus = true;
}

void BeatClockScheduler::processSoon() {
  if (clockTimeoutStatus) {
    clock->clearTimeout(clockTimeoutId);
    clockTimeoutStatus = false;
  }

  clockTimeoutId = clock->setTimeoutAbsolute(0, beatClockSchedulerCallback, this);
  clockTimeoutStatus = true;
}

BeatClockScheduler::Id BeatClockScheduler::setBeatTimeout(double duration,
                                                               BeatClockCbT callback,
                                                               void* callbackUserData) {
  auto now = this->beatInfo->getCurrentBeat();
  auto goal = now + duration;
  return setBeatTimeoutAbsolute(goal, callback, callbackUserData);
}

BeatClockScheduler::Id BeatClockScheduler::setBeatTimeoutAbsolute(double goal,
                                             BeatClockCbT callback,
                                             void* callbackUserData) {
  auto id = idTable.push(BeatClockEvent {
      0, 0, callback, callbackUserData, false
    });
  queue.push(goal, id);
  this->updateWaitingTime();
  return id;
}

BeatClockScheduler::Id BeatClockScheduler::setBeatSyncTimeout(double sync,
                                                                   double offset,
                                                                   BeatClockCbT callback,
                                                                   void* callbackUserData){
  auto now = this->beatInfo->getCurrentBeat();
  auto goal = beatClockSchedulerNextSyncedBeat(now, sync, offset);
  return setBeatTimeoutAbsolute(goal, callback, callbackUserData);
}

BeatClockScheduler::Id BeatClockScheduler::setBeatInterval(double period,
                                      BeatClockCbT callback,
                                      void* callbackUserData){
  auto now = this->beatInfo->getCurrentBeat();
  return setBeatIntervalAbsolute(now + period, period, callback, callbackUserData);
}

BeatClockScheduler::Id BeatClockScheduler::setBeatIntervalAbsolute(double goal,
                                              double period,
                                              BeatClockCbT callback,
                                              void* callbackUserData){
  auto id = idTable.push(BeatClockEvent {
      goal - period, period, callback, callbackUserData, false
    });
  queue.push(goal, id);
  this->updateWaitingTime();
  return id;
}

BeatClockScheduler::Id BeatClockScheduler::setBeatSyncInterval(double sync,
                                                                    double offset,
                                                                    double period,
                                                                    BeatClockCbT callback,
                                                                    void* callbackUserData){
  auto now = this->beatInfo->getCurrentBeat();
  auto goal = beatClockSchedulerNextSyncedBeat(now, sync, offset);
  return setBeatIntervalAbsolute(goal, period, callback, callbackUserData);
}

void BeatClockScheduler::clearBeatTimeout(BeatClockScheduler::Id id) {
  idTable[id].deleted = true;
}

void* BeatClockScheduler::getBeatTimeoutUserData(BeatClockScheduler::Id id) {
  return idTable[id].callbackUserData;
}

void BeatClockScheduler::clearBeatInterval(BeatClockScheduler::Id id) {
  this->clearBeatTimeout(id);
}

void BeatClockScheduler::clearBeatSyncTimeout(BeatClockScheduler::Id id) {
  this->clearBeatTimeout(id);
}

void BeatClockScheduler::processEvent(BeatClockScheduler::Id id, double now, double goal) {
  // at this point, the event is out of the queue, but still in the
  // id table
  BeatClockEvent* event = &idTable[id];
  // callback and reschedule if needed

  if (!event->deleted) {
    BeatClockEventInfo info {id, now, goal, event->period};
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
  } else {
    // we will never need this event again
    idTable.free(id);
  }
}

void BeatClockScheduler::updateWaitingTime() {
  if (queue.size() == 0) {
    this->timer(0, true);
    return;
  }
  const auto& [tbeat, tevent] = queue.top();

  /*
    If more than a tick away, do nothing
  */

  double tickDurationBeats = 1.0 / this->beatInfo->getCurrentPPQN();

  double currentBeat = this->beatInfo->getCurrentBeat();

  if (currentBeat + tickDurationBeats < tbeat) {
    this->timer(0, true);
    return;
  }

  // If already due, schedule immediately
  if (currentBeat >= tbeat) {
    this->processSoon();
    return;
  }

  // If within a tick, schedule at a precise time
  int64_t goal = targetBeatTime(currentBeat, tbeat);
  this->timer(goal);
}

pallet::Time BeatClockScheduler::targetBeatTime(double currentBeat, double targetBeat) {
  auto diff = targetBeat - currentBeat;
  double beatPeriod = this->beatInfo->getCurrentBeatPeriod();
  pallet::Time targetTime = clock->currentTime() + diff * beatPeriod;
  return targetTime;
}

void BeatClockScheduler::process() {
  auto now = this->beatInfo->getCurrentBeat();
  while (true) {
    if (queue.size() == 0) { break; }
    auto [tbeat, teventid] = queue.top();
    if (now < tbeat) {
      break;
    }
    auto [beat, eventid] = queue.top();
    queue.pop();
    processEvent(eventid, now, beat);
  }
  this->updateWaitingTime();
}

void BeatClockScheduler::uponTick() {
  this->process();
}

}
