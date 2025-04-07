#include <stdio.h>
#include <print>
#include "pallet/LinuxPlatform.hpp"
#include "pallet/Clock.hpp"
#include "pallet/measurement.hpp"

void measureTiming(pallet::PosixPlatform& platform, pallet::Clock& clock) {
  auto intervalTime = pallet::timeInMs(100);
  auto meas = pallet::RunningMeanMeasurer<double, 32>();

  auto cb = [&](const pallet::ClockEventInfo& info) {
    auto now = clock.currentTime();
    auto diff = now - info.intended;
    meas.addSample(diff);
    std::println("clock: {}, main: {}, main avg: {}",
                 info.now - info.intended,
                 diff,
                 meas.mean());
  };

  auto simpleForwardingCallback = [](const pallet::ClockEventInfo& info, void* ud) {
       (static_cast<decltype(&cb)>(ud))->operator()(info);
  };

  clock.setInterval(intervalTime,
                    simpleForwardingCallback,
                    &cb);

  while (1) {
    platform.loopIter();
  }
}

int main() {
  auto platformResult = pallet::LinuxPlatform::create();
  if (!platformResult) return 1;
  auto clockResult = pallet::Clock::create(*platformResult);
  auto& platform = *platformResult;
  auto& clock = *clockResult;
  measureTiming(platform, clock);
  return 0;
}
