#include "pallet/BeatClock.hpp"

namespace pallet {

Result<BeatClock> BeatClock::create(Clock& clock) {
  return Result<BeatClock>(std::in_place_t{}, &clock, nullptr);
}

Result<BeatClock> BeatClock::create(Clock& clock, MidiInterface& midiInterface) {
  return Result<BeatClock>(std::in_place_t{}, &clock, &midiInterface);
}

BeatClock::BeatClock(Clock* clock, MidiInterface* midiInterface) {
  this->clock = clock;
  this->midiInterface = midiInterface;
  this->implementation = nullptr;

  internalImplementation.init(clock, midiInterface);
  internalScheduleInfo.init(&internalImplementation);
  midiImplementation.init(clock, midiInterface);
  midiScheduleInfo.init(&midiImplementation);

  auto tickCallback = [](BeatClockInfo* info, void* ud) {
    (void)info;
    auto bc = static_cast<BeatClock*>(ud);
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

BeatClock::Id BeatClock::setBeatSyncTimeout(double sync,
                           double offset,
                           Callback callback){
  return scheduler.
    setBeatSyncTimeout(sync, offset, std::move(callback));
}

BeatClock::Id BeatClock::setBeatSyncInterval(double sync,
                            double offset,
                            double period,
                            Callback callback)  {
  return scheduler.
    setBeatSyncInterval(sync, offset, period, std::move(callback));
}

void BeatClock::clearBeatSyncTimeout(BeatClock::Id id) {
  return scheduler.
    clearBeatSyncTimeout(id);
}

void BeatClock::clearBeatSyncInterval(BeatClock::Id id) {
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
