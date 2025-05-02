#include "pallet/LinuxPlatform.hpp"
#include "pallet/Clock.hpp"
#include "pallet/PosixGraphicsInterface.hpp"
#include "pallet/LuaInterface.hpp"
#include "pallet/BeatClock.hpp"
#include "pallet/OscMonomeGridInterface.hpp"
#include "pallet/LoOscInterface.hpp"
#include "pallet/io.hpp"

int cliMain(const char* filename) {
  auto res = pallet::LuaInterface::create().and_then([&](auto&& luaInterface) {
    return pallet::LinuxPlatform::create().and_then([&](auto&& platform) {
      return pallet::Clock::create(platform).and_then([&](auto&& clock) {
        return pallet::BeatClock::create(clock).and_then([&](auto&& beatClock) {
          return pallet::PosixMidiInterface::create(platform).and_then([&](auto&& midiInterface) {
            return pallet::PosixGraphicsInterface::create(platform).and_then([&](auto&& graphicsInterface) {
              return pallet::LoOscInterface::create(platform).and_then([&](auto&& oscInterface) {
                return pallet::OscMonomeGridInterface::create(oscInterface).and_then([&](auto&& gridInterface) {
                  luaInterface.setPlatform(platform);
                  luaInterface.setClockInterface(clock);
                  luaInterface.setBeatClock(beatClock);
                  luaInterface.setGraphicsInterface(graphicsInterface);
                  luaInterface.setMidiInterface(midiInterface);
                  luaInterface.setMonomeGridInterface(gridInterface);
                  luaInterface.setOscInterface(oscInterface);
                  return pallet::readFile(filename).transform([&](auto&& contents) {
                    luaInterface.dostring(contents.c_str());
                    platform.loop();
                  });
                });
              });
            });
          });
        });
      });
    });
  });

  if (!res) { return 1; }
  return 0;
}
