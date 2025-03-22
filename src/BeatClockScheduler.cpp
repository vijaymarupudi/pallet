#include "BeatClockScheduler.hpp"
#include <cmath>

namespace pallet {

static void beatClockSchedulerCallback(ClockEventInfo* info, void* ud) {
  (void)info;
  // This timer has expired
  ((BeatClockScheduler*)ud)->clockTimeoutStatus = false;
  ((BeatClockScheduler*)ud)->process();
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

BeatClockScheduler::id_type BeatClockScheduler::setBeatTimeout(double duration,
                                                               BeatClockCbT callback,
                                                               void* callbackUserData) {
  auto now = this->beatInfo->getCurrentBeat();
  auto goal = now + duration;
  return setBeatTimeoutAbsolute(goal, callback, callbackUserData);
}

BeatClockScheduler::id_type BeatClockScheduler::setBeatTimeoutAbsolute(double goal,
                                             BeatClockCbT callback,
                                             void* callbackUserData) {
  auto id = idTable.push(BeatClockEvent {
      0, 0, callback, callbackUserData, false
    });
  queue.push(goal, id);
  this->updateWaitingTime();
  return id;
}

BeatClockScheduler::id_type BeatClockScheduler::setBeatSyncTimeout(double sync,
                                                                   double offset,
                                                                   BeatClockCbT callback,
                                                                   void* callbackUserData){
  auto now = this->beatInfo->getCurrentBeat();
  auto goal = beatClockSchedulerNextSyncedBeat(now, sync, offset);
  return setBeatTimeoutAbsolute(goal, callback, callbackUserData);
}

BeatClockScheduler::id_type BeatClockScheduler::setBeatInterval(double period,
                                      BeatClockCbT callback,
                                      void* callbackUserData){
  auto now = this->beatInfo->getCurrentBeat();
  return setBeatIntervalAbsolute(now + period, period, callback, callbackUserData);
}

BeatClockScheduler::id_type BeatClockScheduler::setBeatIntervalAbsolute(double goal,
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

BeatClockScheduler::id_type BeatClockScheduler::setBeatSyncInterval(double sync,
                                                                    double offset,
                                                                    double period,
                                                                    BeatClockCbT callback,
                                                                    void* callbackUserData){
  auto now = this->beatInfo->getCurrentBeat();
  auto goal = beatClockSchedulerNextSyncedBeat(now, sync, offset);
  return setBeatIntervalAbsolute(goal, period, callback, callbackUserData);
}

void BeatClockScheduler::clearBeatTimeout(BeatClockScheduler::id_type id) {
  idTable[id].deleted = true;
}

void* BeatClockScheduler::getBeatTimeoutUserData(BeatClockScheduler::id_type id) {
  return idTable[id].callbackUserData;
}

void BeatClockScheduler::clearBeatInterval(BeatClockScheduler::id_type id) {
  this->clearBeatTimeout(id);
}

void BeatClockScheduler::clearBeatSyncTimeout(BeatClockScheduler::id_type id) {
  this->clearBeatTimeout(id);
}

void BeatClockScheduler::processEvent(BeatClockScheduler::id_type id, double now, double goal) {
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
