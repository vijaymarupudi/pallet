#pragma once
#include <array>
#include <inttypes.h>
#include "Clock.hpp"
#include "utils.hpp"
#include "BeatClockScheduler.hpp"
#include "macros.hpp"
#include "MidiInterface.hpp"

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

  T mean() {
    T total = 0;

    if (len == maxLen) {
      for (int i = 0; i < len; i++) {
        total += measurements[i] / maxLen;
      }
    } else {
      for (int i = 0; i < len; i++) {
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

constexpr PALLET_ALWAYS_INLINE uint64_t bpmToBeatPeriod(double bpm) {
  return pallet::timeInS(60.0 / bpm);
}

constexpr PALLET_ALWAYS_INLINE uint64_t bpmToPPQNPeriod(double bpm, int ppqn) {
  return pallet::timeInS(60.0 / bpm / ppqn);
}

class BeatClockMeasurement {
  using FloatingPoint = double;
public:
  BeatClockMeanMeasurer<FloatingPoint, 32> measurer;
  FloatingPoint phase = 0;
  int phaseCounter = 0;
 
  FloatingPoint timePerBeat = 0.5;
  int ppqn = 24;
  uint64_t prev = 0;
  void init() { reset(); };
  void tick(uint64_t time) {
    if (prev == 0) {
      prev = time;
      return; 
    }
    uint64_t diff = time - prev;
    measurer.addSample(diff);
    timePerBeat = measurer.mean() * ppqn;
    prev = time;
    
    phase = (phase + 1.0/ppqn);
    phaseCounter = phaseCounter + 1;
    if (phaseCounter == ppqn) {
      phase = 0;
      phaseCounter = 0;
    }
  };

  bool isValid() {
    return measurer.len > 0;
  };

  void reset() {
    phaseCounter = ppqn - 1;
    phase = 1.0/ppqn * phaseCounter;
  }

  void clear() {
    reset();
    measurer.clear();
    prev = 0;
  }
};

struct BeatClockInfo {
  double bpm;
  int ppqn;
  double beat;
  int beatPhase;
  uint64_t time;
  uint64_t intended;
};

enum class BeatClockType {
  Internal,
  Midi
};

enum class BeatClockTransportType {
  Start,
  Stop,
  Continue
};

class BeatClockImplementationInterface {
public:
  pallet::CStyleCallback<BeatClockInfo*> onTickCallback;
  pallet::CStyleCallback<BeatClockTransportType> onTransportCallback;
  Clock* clock;
  bool active;

  // Clock state info

  // Update setStateFromOther if you add more state
  double bpm = 120;
  int ppqn = 24;
  uint64_t beatPeriod = bpmToBeatPeriod(120);
  uint64_t ppqnPeriod = bpmToPPQNPeriod(120, 24);
  double beat = 0;
  int beatRef = 0; // integer component of beat
  int tickCount = 0;
  int beatPhase = 0;
  uint64_t lastTickTime = 0;
  uint64_t lastTickTimeIntended = 0;

  void init(Clock* clock) {
    this->clock = clock;
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

  void uponTick(uint64_t time, uint64_t intended) {
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
  
  virtual void setBPM(double bpm) {
    this->bpm = bpm;
    this->beatPeriod = bpmToBeatPeriod(this->bpm);
    this->ppqnPeriod = bpmToPPQNPeriod(this->bpm, this->ppqn);
  };

  virtual void setPPQN(int ppqn) {
    this->ppqn = ppqn;
    this->ppqnPeriod = bpmToPPQNPeriod(this->bpm, this->ppqn);
  }

  virtual void setActive(bool activeState) {
    this->active = activeState;
  }

};

class BeatClockInternalImplementation : public BeatClockImplementationInterface  {
public:
  Clock::id_type interval;
  bool running = false;
  Clock* clock;

  void init(Clock* clock) {
    BeatClockImplementationInterface::init(clock);
  }

  void run() {
    running = true;
  }

  void runClockIfNecessary() {
    auto now = clock->currentTime();
    auto cb = [](ClockEventInfo* info, void* ud) {
      auto bc = ((BeatClockInternalImplementation*)ud);
      bc->uponTick(info->now, info->intended);
    };
    this->interval = clock->setIntervalAbsolute(now,
                                                beatPeriod,
                                                cb,
                                                this);
  }

  void stopRunning() {
    running = false;
    clock->clearInterval(interval);
  }

  void cleanup() {
    if (running) {
      running = false;
      clock->clearInterval(interval);
    }
  }

  virtual void setBPM(double bpm) {
    auto old = this->ppqnPeriod;
    BeatClockImplementationInterface::setBPM(bpm);
    if (old != this->ppqnPeriod) {
      if (running) {
        
      }
    }
  }
};

class BeatClockInternalSchedulerInformationInterface : public BeatClockSchedulerInformationInterface {
  BeatClockInternalImplementation* bc;
  void init(BeatClockInternalImplementation* bc) {
    this->bc = bc;
  }
  virtual double getCurrentBeat() {
    auto beatPeriod = bc->beatPeriod;
    auto now = bc->clock->currentTime();
    auto timeSinceLastTick = now - bc->lastTickTimeIntended;
    return bc->beat + timeSinceLastTick / beatPeriod;
  };
  virtual double getCurrentBeatPeriod() {
    return bc->beatPeriod;
  };
  virtual int getCurrentPPQN() {
    return bc->ppqn;
  };
};

class BeatClockMidiImplementation : public BeatClockImplementationInterface {
public:
  MidiInterface* mface;
  void init(Clock* clock,
            MidiInterface* mface) {
    BeatClockImplementationInterface::init(clock);
    this->mface = mface;
  }

  void cleanup() {
    
  }
};

class BeatClockMidiSchedulerInformationInterface : public BeatClockSchedulerInformationInterface {
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
  BeatClockType mode;
  BeatClockImplementationInterface* implementation = nullptr;
  BeatClockScheduler scheduler;
  BeatClockInternalImplementation internalImplementation;
  BeatClockInternalSchedulerInformationInterface internalScheduleInfo;
  BeatClockMidiImplementation midiImplementation;
  BeatClockMidiSchedulerInformationInterface midiScheduleInfo;
  
  void init(Clock* clock, MidiInterface* midiInterface) {
    this->clock = clock;
    this->implementation = nullptr;
    internalImplementation.init(clock);
    midiImplementation.init(clock, midiInterface);
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
      oldImplementation->setActive(false);
      implementation->setStateFromOther(*oldImplementation);
      implementation->setActive(true);
    }
  }

  id_type setBeatSyncTimeout(double sync,
                             double offset,
                             BeatClockCbT callback,
                             void* callbackUserData) {
    return scheduler.
      setBeatSyncTimeout(sync, offset, callback, callbackUserData);
  }
  
  void clearBeatSyncTimeout(id_type id) {
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
