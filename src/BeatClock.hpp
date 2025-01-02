#pragma once
#include <array>
#include <inttypes.h>
#include "Clock.hpp"
#include "utils.hpp"
#include "BeatClockScheduler.hpp"
#include "macros.hpp"
#include "MidiInterface.hpp"

namespace pallet {

template <class T, size_t maxLen>
struct BeatClockMeanMeasurer {
  std::array<T, maxLen> measurements = {0};
  size_t len = 0;
  size_t i = 0;

  void addSample(T sample) {
    measurements[i] = sample;
    i = (i + 1) % maxLen;
    if (len < maxLen) {
      len++;
    }
  }

  T mean(double defaultValue) {

    if (len == 0) {
      return defaultValue;
    }

    T total = 0;
    if (len == maxLen) {
      for (size_t i = 0; i < len; i++) {
        total += measurements[i] / maxLen;
      }
    } else {
      for (size_t i = 0; i < len; i++) {
        total += measurements[i] / len;
      }
    }

    return total;
  }

  void clear() {
    len = 0;
    i = 0;
  }
};

constexpr PALLET_ALWAYS_INLINE pallet::Time bpmToBeatPeriod(double bpm) {
  return pallet::timeInS(60.0 / bpm);
}

constexpr PALLET_ALWAYS_INLINE pallet::Time bpmToPPQNPeriod(double bpm, int ppqn) {
  return pallet::timeInS(60.0 / bpm / ppqn);
}

struct BeatClockInfo {
  double bpm;
  int ppqn;
  double beat;
  int beatPhase;
  pallet::Time time;
  pallet::Time intended;
};

enum class BeatClockType {
  Internal,
  Midi
};

enum class BeatClockTransportType {
  Start,
  Stop,
  Reset
};

class BeatClockImplementationInterface {
public:
  pallet::CStyleCallback<BeatClockInfo*> onTickCallback;
  pallet::CStyleCallback<BeatClockTransportType> onTransportCallback;
  Clock* clock;
  MidiInterface* midiInterface;
  bool running = false;

  // Clock state info

  // Update setStateFromOther if you add more state
  double bpm = 120;
  int ppqn = 24;
  pallet::Time beatPeriod = bpmToBeatPeriod(120);
  pallet::Time ppqnPeriod = bpmToPPQNPeriod(120, 24);
  double beat = 0;
  int beatRef = 0; // integer component of beat
  int tickCount = 0;
  int beatPhase = 0;
  pallet::Time lastTickTime = 0;
  pallet::Time lastTickTimeIntended = 0;

  void init(Clock* clock, MidiInterface* midiInterface) {
    this->clock = clock;
    this->midiInterface = midiInterface;
  }

  void setStateFromOther(BeatClockImplementationInterface& other) {
    this->bpm = other.bpm;
    this->ppqn = other.ppqn;
    this->beatPeriod = other.beatPeriod;
    this->ppqnPeriod = other.ppqnPeriod;
    this->beat = other.beat;
    this->beatRef = other.beatRef; // integer component of beat
    this->tickCount = other.tickCount;
    this->beatPhase = other.beatPhase;
    this->lastTickTime = other.lastTickTime;
    this->lastTickTimeIntended = other.lastTickTimeIntended;
  }

  void setOnTick(auto&& onTickCb,
                 auto&& onTickUserData) {
    onTickCallback.set(onTickCb, onTickUserData);
  }

  void setOnTransport(auto&& onT, auto&& data) {
    onTransportCallback.set(onT, data);
  }

  void uponTick(pallet::Time time, pallet::Time intended) {
    // printf("BeatClock::uponTick() | tick: %lu, intended: %lu, beat: %f\n", time, intended, beat);
    BeatClockInfo info {
      this->bpm,
      this->ppqn,
      this->beat,
      this->beatPhase,
      time,
      intended
    };
    onTickCallback.call(&info);
    tickCount += 1;
    beatPhase += 1;
    beat += 1.0/ppqn;
    lastTickTime = time;
    lastTickTimeIntended = intended;
    if (beatPhase % ppqn == 0) {
      beatPhase = 0;
      beatRef += 1;
      beat = beatRef;
    }
  };

  void uponTransport(BeatClockTransportType transport) {
    onTransportCallback.call(transport);
  }

  virtual void run(bool state) {
    running = state;
  }

  virtual void setBPM(double bpm) {
    this->bpm = bpm;
    this->beatPeriod = bpmToBeatPeriod(this->bpm);
    this->ppqnPeriod = bpmToPPQNPeriod(this->bpm, this->ppqn);
  };

  virtual void setPPQN(int ppqn) {
    this->ppqn = ppqn;
    this->ppqnPeriod = bpmToPPQNPeriod(this->bpm, this->ppqn);
  }



};

class BeatClockInternalImplementation : public BeatClockImplementationInterface  {
public:
  Clock::id_type interval;

  virtual void run(bool state) override {
    bool oldState = running;
    BeatClockImplementationInterface::run(state);
    if (oldState == state) { return; }
    if (state) {
      // running, start tick
      auto now = clock->currentTime();
      this->startTickInterval(now);
    } else {
      // not running
      clock->clearInterval(this->interval);
    }
  }

private:
  void startTickInterval(pallet::Time startTime) {
    auto cb = [](ClockEventInfo* info, void* ud) {
      auto bc = ((BeatClockInternalImplementation*)ud);
      bc->uponTick(info->now, info->intended);
    };
    this->lastTickTimeIntended = startTime;
    this->interval = clock->setIntervalAbsolute(startTime,
                                                this->ppqnPeriod,
                                                cb,
                                                this);
  }

public:
  void cleanup() {
    this->run(false);
  }

  virtual void setBPM(double bpm) {
    auto old = this->ppqnPeriod;
    BeatClockImplementationInterface::setBPM(bpm);
    if (old != this->ppqnPeriod) {
      if (this->running) {
        clock->clearInterval(this->interval);

        auto lastTickTimeIntended = this->lastTickTimeIntended;
        auto now = clock->currentTime();

        pallet::Time startTime;
        if (lastTickTimeIntended + this->ppqnPeriod >= now) {
          startTime = lastTickTimeIntended + this->ppqnPeriod;
        } else {
          startTime = now;
        }
        this->startTickInterval(startTime);
      }

      // if it's not running, then calling the superclasses setBPM should be enough
    }
  }
};

class BeatClockInternalSchedulerInformationInterface : public BeatClockSchedulerInformationInterface {
public:
  BeatClockInternalImplementation* bc;
  void init(BeatClockInternalImplementation* bc) {
    this->bc = bc;
  }
  virtual double getCurrentBeat() {
    auto beatPeriod = bc->beatPeriod;
    auto now = bc->clock->currentTime();
    auto timeSinceLastTickIntended = now - bc->lastTickTimeIntended;
    double b = bc->beat + (double)timeSinceLastTickIntended / (double)beatPeriod;
    // printf("timeSinceLastTickIntended: %lu, beat: %f, current beat: %f %f\n", timeSinceLastTickIntended,
    //        bc->beat, b, (double)timeSinceLastTickIntended / (double)beatPeriod);
    return b;
  };
  virtual double getCurrentBeatPeriod() {
    return bc->beatPeriod;
  };
  virtual int getCurrentPPQN() {
    return bc->ppqn;
  };
};

class BeatClockMidiImplementation : public BeatClockImplementationInterface {
  BeatClockMeanMeasurer<double, 32> meanMeasurer;
public:
  virtual void run(bool state) override {
    if (state) {
      auto cb = [](pallet::Time time, const unsigned char* buf, size_t len, void* ud) {
        auto bc = (BeatClockMidiImplementation*)ud;
        bc->uponMidiTick(time, buf, len);
      };
      this->midiInterface->setOnMidiClock(cb, this);
    } else {
      this->midiInterface->setOnMidiClock(nullptr, nullptr);
    }
  }
  void uponMidiTick(pallet::Time time, const unsigned char* buf, size_t len) {
    if (!(len == 1 && buf[0] == 0xF8)) { return; }
    auto sample = time - this->lastTickTime;
    meanMeasurer.addSample(sample);
    this->ppqnPeriod =  meanMeasurer.mean(1.0 / 120 / 24);//default period
    this->beatPeriod = ppqnPeriod * 24;
    this->bpm = 1.0 / pallet::timeToS(this->beatPeriod) * 60;
    printf("bpm %f\n", bpm);
    this->uponTick(time, time);
  }

  virtual void setBPM(double bpm) override {
    (void)bpm;
    // do nothing
  }

  void cleanup() {
    this->run(false);
  }
};

class BeatClockMidiSchedulerInformationInterface : public BeatClockSchedulerInformationInterface {
public:
  BeatClockMidiImplementation* bc;
  void init(BeatClockMidiImplementation* bc) {
    this->bc = bc;
  }
  virtual double getCurrentBeat() {
    auto beatPeriod = bc->beatPeriod;
    auto now = bc->clock->currentTime();
    auto timeSinceLastTick = now - bc->lastTickTime;
    return bc->beat + timeSinceLastTick / beatPeriod;
  };
  virtual double getCurrentBeatPeriod() {
    return bc->beatPeriod;
  };
  virtual int getCurrentPPQN() {
    return bc->ppqn;
  };
};

class BeatClock {
public:
  using id_type = BeatClockScheduler::id_type;

  Clock* clock;
  MidiInterface* midiInterface;
  BeatClockScheduler scheduler;

  bool _sendMidiClock = false;

  BeatClockType mode;
  BeatClockImplementationInterface* implementation = nullptr;

  BeatClockInternalImplementation internalImplementation;
  BeatClockInternalSchedulerInformationInterface internalScheduleInfo;

  BeatClockMidiImplementation midiImplementation;
  BeatClockMidiSchedulerInformationInterface midiScheduleInfo;

  void init(Clock* clock, MidiInterface* midiInterface) {
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

  void setClockSource(BeatClockType mode) {
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

  void uponTick() {
    if (this->_sendMidiClock && this->midiInterface) {
      unsigned char msg[] = {0xF8};
      this->midiInterface->sendMidi(msg, 1);
    }
    this->scheduler.uponTick();

  }

  void sendMidiClock(bool state) {
    // 0 to disable
    this->_sendMidiClock = state;
  }

  id_type setBeatSyncTimeout(double sync,
                             double offset,
                             BeatClockCbT callback,
                             void* callbackUserData){
    return scheduler.
      setBeatSyncTimeout(sync, offset, callback, callbackUserData);
  }

  id_type setBeatSyncInterval(double sync,
                              double offset,
                              double period,
                              BeatClockCbT callback,
                              void* callbackUserData)  {
    return scheduler.
      setBeatSyncInterval(sync, offset, period, callback, callbackUserData);
  }

  void clearBeatSyncTimeout(id_type id) {
    return scheduler.
      clearBeatSyncTimeout(id);
  }

  void clearBeatSyncInterval(id_type id) {
    return scheduler.
      clearBeatSyncTimeout(id);
  }

  void setBPM(double bpm) {
    return implementation->setBPM(bpm);
  }

  void cleanup() {
    internalImplementation.cleanup();
    midiImplementation.cleanup();
  }
};

}
