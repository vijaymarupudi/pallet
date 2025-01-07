#include <cstdio>
#include "Platform.hpp"
#include "Clock.hpp"

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
