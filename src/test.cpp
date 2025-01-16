#include <cstdio>
#include "Platform.hpp"
#include "Clock.hpp"
#include "time.hpp"
#include <string>


int main()
  
{
  auto platform = pallet::LinuxPlatform();
  auto clock = pallet::Clock(platform);

  while (true) {
    platform.loopIter();
  }
  
  // Wow var;
  return 0;
}
