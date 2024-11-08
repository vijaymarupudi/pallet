#pragma once
#include <array>
#include <inttypes.h>
#include "Clock.hpp"
#include "utils.hpp"
#include "BeatClockScheduler.hpp"

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

static uint64_t bpmToBeatPeriod(double bpm) {
  return pallet::timeInS(60.0 / bpm);
}

static uint64_t bpmToPPQNPeriod(double bpm, int ppqn) {
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
  double beatPhase;
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
  using id_type = BeatClockScheduler::id_type;
  CStyleCallback<BeatClockInfo*> onTickCallback;
  CStyleCallback<BeatClockTransportType> onTransportCallback;
  BeatClockScheduler* scheduler;
  Clock* clock;

  double bpm = 120;
  int ppqn = 24;
  uint64_t beatPeriod = 500000;
  uint64_t ppqnPeriod = 20833;
  double beat = 0;
  int beatRef = 0; // integer component of beat
  int tickCount = 0;
  int beatPhase = 0;
  uint64_t lastTickTime = 0;

  void init(Clock* clock, BeatClockScheduler* scheduler) : clock(clock), scheduler(scheduler) {
  }

  void setOnTick(auto&& onTickCb,
                 auto&& onTickUserData) {
    onTickCallback.set(onTickCb, onTickUserData);
  }

  void setOnTransport(auto&& onT, auto&& data) {
    onTransportCallback.set(onT, data);
  }

  void uponTick(uint64_t time, uint64_t intended) {
    BeatClockInfo info { bpm, ppqn, beat, beatPhase, time, intended };
    onTickCallback.call(&info, this->onTickUserData);
    tickCount += 1;
    beatPhase += 1;
    beat += 1.0/ppqn;
    if (beatPhase % ppqn == 0) {
      beatPhase = 0;
      beatRef += 1;
      beat = beatRef;
    }
  };

  void uponTransport(BeatClockTransport transport) {
    onTransportCallback.call(transport);
  }
  
  virtual setBPM(double bpm) {
    this->bpm = bpm;
    this->beatPeriod = bpmToPeriod(this->bpm);
    this->ppqnPeriod = bpmToPPQNPeriod(this->bpm, this->ppqn);
  };

  virtual setPPQN(int ppqn) {
    this->ppqn = ppqn;
    this->ppqnPeriod = bpmToPPQNPeriod(this->bpm, this->ppqn);
  }
};

class BeatClockInternal : public BeatClockImplementationInterface  {
public:
  Clock::id_type interval;
  bool running = false;
  Clock* clock;

  void init(Clock* clock) {
    BeatClockImplementationInterface::init(clock);
    this->setBPM(120);
  }

  void run() {
    running = true;
    auto now = clock->currentTime();
    auto cb = [](ClockEventInfo* info, void* ud) {
      auto bc = ((BeatClockInternal*)ud);
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

  virtual setBPM(double bpm) {
    auto old = this->ppqnPeriod;
    BeatClockImplementationInterface::setBPM(bpm);
    if (old != this->ppqnPeriod) {
      if (running) {
        
      }
    }
  }
};

class BeatClockMidi : public BeatClockImplementationInterface {
};


class BeatClock {
  BeatClockType mode;
  BeatClockImplementationInterface* implementation;
  BeatClockInternal internalImplementation;
  BeatClockMidi midiImplementation;
  
  void init(Clock* clock) {
    this->clock = clock;
    mode = BeatClockType::Internal;
    internalImplementation.init(clock);
    midiImplementation.init(clock);
    implementation = internalImplementation;
  }

  id_type setBeatSyncTimeout(double sync,
                             double offset,
                             BeatClockCbT callback,
                             void* callbackUserData) {
    return implementation->
      scheduler->
      setBeatSyncTimeout(sync, offset, callback, callbackUserData);
  }
  
  void clearBeatSyncTimeout(id_type id) {
    return implementation->
      scheduler->
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
