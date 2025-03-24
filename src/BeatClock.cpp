#include "BeatClock.hpp"

namespace pallet {

BeatClock::BeatClock(Clock& clock) {
  this->_init(&clock, nullptr);
}

BeatClock::BeatClock(Clock& clock, MidiInterface& midiInterface) {
  this->_init(&clock, &midiInterface);
}

void BeatClock::_init(Clock* clock, MidiInterface* midiInterface) {
  this->clock = clock;
  this->midiInterface = midiInterface;
  this->implementation = nullptr;

  internalImplementation.init(clock, midiInterface);
  internalScheduleInfo.init(&internalImplementation);
  midiImplementation.init(clock, midiInterface);
  midiScheduleInfo.init(&midiImplementation);

  auto tickCallback = [](BeatClockInfo* info, void* ud) {
    (void)info;
    auto bc = (BeatClock*)ud;
    bc->uponTick();
  };

  internalImplementation.onTickCallback.set(tickCallback, this);
  midiImplementation.onTickCallback.set(tickCallback, this);

  this->scheduler.init(clock, &internalScheduleInfo);
  this->setClockSource(BeatClockType::Internal);
  // now implementation is set
  this->implementation->setBPM(120);
}

void BeatClock::setClockSource(BeatClockType mode) {
  auto oldImplementation = implementation;
  this->mode = mode;
  switch (mode) {
  case BeatClockType::Internal:
    implementation = &internalImplementation;
    break;
  case BeatClockType::Midi:
    implementation = &midiImplementation;
    break;
  }
  if (oldImplementation) {
    // we're switching clocks
    oldImplementation->run(false);
    implementation->setStateFromOther(*oldImplementation);
  }

  // switch the beat info of the scheduler
  switch(mode) {
  case BeatClockType::Internal:
    scheduler.setBeatInfo(&internalScheduleInfo);
    break;
  case BeatClockType::Midi:
    scheduler.setBeatInfo(&midiScheduleInfo);
    break;
  }

  implementation->run(true);
}

void BeatClock::uponTick() {
  if (this->_sendMidiClock && this->midiInterface) {
    unsigned char msg[] = {0xF8};
    this->midiInterface->sendMidi(msg, 1);
  }
  this->scheduler.uponTick();

}

void BeatClock::sendMidiClock(bool state) {
  // 0 to disable
  this->_sendMidiClock = state;
}

BeatClock::id_type BeatClock::setBeatSyncTimeout(double sync,
                           double offset,
                           BeatClockCbT callback,
                           void* callbackUserData){
  return scheduler.
    setBeatSyncTimeout(sync, offset, callback, callbackUserData);
}

BeatClock::id_type BeatClock::setBeatSyncInterval(double sync,
                            double offset,
                            double period,
                            BeatClockCbT callback,
                            void* callbackUserData)  {
  return scheduler.
    setBeatSyncInterval(sync, offset, period, callback, callbackUserData);
}

void BeatClock::clearBeatSyncTimeout(BeatClock::id_type id) {
  return scheduler.
    clearBeatSyncTimeout(id);
}

void BeatClock::clearBeatSyncInterval(BeatClock::id_type id) {
  return scheduler.
    clearBeatSyncTimeout(id);
}

void BeatClock::setBPM(double bpm) {
  return implementation->setBPM(bpm);
}

void BeatClock::cleanup() {
  internalImplementation.cleanup();
  midiImplementation.cleanup();
}

double BeatClock::currentBeat() {
  return scheduler.getCurrentBeat();
}

}
