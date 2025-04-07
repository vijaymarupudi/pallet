#pragma once
#include <cstdint>
#include "pallet/Clock.hpp"
#include "pallet/utils.hpp"
#include "pallet/BeatClockScheduler.hpp"
#include "pallet/macros.hpp"
#include "pallet/MidiInterface.hpp"
#include "pallet/measurement.hpp"

namespace pallet {


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

  virtual void init(Clock* clock, MidiInterface* midiInterface) {
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
  Clock::Id interval;

  // virtual void init(Clock* clock, MidiInterface* midi) override {
  //   BeatClockImplementationInterface::init(clock, midi);
  // }

  virtual void run(bool state) override {
    bool oldState = running;
    BeatClockImplementationInterface::run(state);
    if (oldState == state) { return; }
    if (state) {
      // running, start tick

      auto now = clock->currentTime();

      // this is used to calculate the current beat
      this->lastTickTimeIntended = now;

      this->startTickInterval(now);
    } else {
      // not running
      clock->clearInterval(this->interval);
    }
  }

private:
  void startTickInterval(pallet::Time startTime) {
    auto cb = [](const ClockEventInfo& info, void* ud) {
      auto bc = static_cast<BeatClockInternalImplementation*>(ud);
      bc->uponTick(info.now, info.intended);
    };
    this->interval = clock->setIntervalAbsolute(startTime,
                                                this->ppqnPeriod,
                                                {cb, this});
  }

public:
  void cleanup() {
    this->run(false);
  }

  virtual void setBPM(double bpm) override {
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
    // std::println("currentBeat|now: {}, intended: {}, diff: {}",
    //              now, bc->lastTickTimeIntended,
    //              timeSinceLastTickIntended);

    double b = bc->beat + (double)timeSinceLastTickIntended / (double)beatPeriod;
     // printf("timeSinceLastTickIntended: %lu, beat: %f, current beat: %f %f\n", timeSinceLastTickIntended,
     //       bc->beat, b, (double)timeSinceLastTickIntended / (double)beatPeriod);
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
  RunningMeanMeasurer<double, 32> meanMeasurer;
public:
  virtual void run(bool state) override {
    if (state) {
      auto cb = [](pallet::Time time, const unsigned char* buf, size_t len, void* ud) {
        auto bc = static_cast<BeatClockMidiImplementation*>(ud);
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
    this->ppqnPeriod =  meanMeasurer.mean() == 0 ? (1.0 / 120 / 24) : meanMeasurer.mean();
    this->beatPeriod = ppqnPeriod * 24;
    this->bpm = 1.0 / pallet::timeToS(this->beatPeriod) * 60;
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
  using Id = BeatClockScheduler::Id;
  using Callback = BeatClockScheduler::Callback;

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


  static Result<BeatClock> create(Clock& clock);
  static Result<BeatClock> create(Clock& clock, MidiInterface& midiInterface);
  
  BeatClock(Clock* clock, MidiInterface* midiInterface);
  void setClockSource(BeatClockType mode);
  void uponTick();
  void sendMidiClock(bool state);
  Id setBeatSyncTimeout(double sync,
                        double offset,
                        Callback callback);
  Id setBeatSyncInterval(double sync,
                         double offset,
                         double period,
                         Callback callback);
  void clearBeatSyncTimeout(Id id);
  void clearBeatSyncInterval(Id id);
  void setBPM(double bpm);
  void cleanup();
  double currentBeat();

};

}
