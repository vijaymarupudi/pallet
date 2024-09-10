#pragma once

namespace pallet {
  namespace constants {
#ifdef RASPBERRYPI_PICO
#define PALLET_CONSTANTS_IS_EMBEDDED_DEVICE 1
    constexpr bool isEmbeddedDevice = true;
#else
    constexpr bool isEmbeddedDevice = false;
#endif
  }
}
