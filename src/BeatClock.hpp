#pragma once
#include <array>
#include <inttypes.h>
#include "Clock.hpp"

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


uint64_t bpmTo24PPQNPeriod(double bpm) {
  return 60 / (double)bpm / 24 * 1000 * 1000;
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

enum class BeatClockMode {
  Internal,
  Midi
};

class BeatClock {
  using BeatClockOnTickT = void(*)(uint64_t);
  Clock* clock;
  double bpm;
  void uponTick(uint64_t time) {
    
  };
  virtual setBPM(float bpm) = 0;
};

class BeatClockInternal : public BeatClock  {
  Clock::id_type interval;
  uint64_t intervalDuration;
  double bpm;
  Clock* clock;
  void init(Clock* clock, double bpm) : clock(clock), bpm(bpm) {
    intervalDuration = bpmTo24PPQNPeriod(bpm);
    auto now = clock->currentTime();
    auto cb = [](ClockEventInfo* info, void* ud) {
      auto bc = ((BeatClockInternal*)ud);
      bc->tick(info->intended);
    };
    this->interval = clock->setIntervalAbsolute(now,
                                                intervalDuration,
                                                cb,
                                                this);
  }

  void start()
  
};


class BeatClockInterface {
  Clock* clock;
  BeatClockMeasurement measurement;
  float internalBPM;
  BeatClockMode mode;
  

  void init(Clock* clock) {
    this->clock = clock;
    mode = BeatClockMode::Internal;
    internalBPM = 120;
  }
  
  

  void cleanup() {
    
  }
};
