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
  // auto beatClockResult = pallet::BeatClock::create(*clockResult);
  // auto midiInterfaceResult = pallet::PosixMidiInterface::create(*platformResult);
  // auto graphicsInterfaceResult = pallet::PosixGraphicsInterface::create(*platformResult);
  // auto oscInterfaceResult = pallet::LoOscInterface::create(*platformResult);
  // if (!oscInterfaceResult) return 1;
  // auto gridInterfaceResult = pallet::OscMonomeGridInterface::create(*oscInterfaceResult);
  // auto luaInterfaceResult = pallet::LuaInterface::create();

  //   if (!(platformResult && clockResult && midiInterfaceResult && beatClockResult && graphicsInterfaceResult && oscInterfaceResult && gridInterfaceResult && luaInterfaceResult)) {
  //   return 1;
  // }

  // auto& luaInterface = *luaInterfaceResult;
  // luaInterface.setClockInterface(*clockResult);
  // luaInterface.setBeatClock(*beatClockResult);
  // luaInterface.setGraphicsInterface(*graphicsInterfaceResult);
  // luaInterface.setMidiInterface(*midiInterfaceResult);
  // luaInterface.setMonomeGridInterface(*gridInterfaceResult);
  // luaInterface.setOscInterface(*oscInterfaceResult);
 
  // auto res = pallet::readFile(filename).transform([&](auto&& contents) {
  //   luaInterface.dostring(contents.c_str());
  //   (*platformResult).loop();
  // });

  if (!res) { return 1; }
  return 0;
}
