#pragma once

namespace pallet {
  namespace constants {
    enum class platforms {
      Pico,
      Linux
    };

#define PALLET_CONSTANTS_PLATFORM_LINUX 0
#define PALLET_CONSTANTS_PLATFORM_PICO 1

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#define PALLET_CONSTANTS_PLATFORM_IS_POSIX
#endif

#if defined(RASPBERRYPI_PICO)
#define PALLET_CONSTANTS_PLATFORM PALLET_CONSTANTS_PLATFORM_PICO
    constexpr platforms currentPlatform = platforms::Pico;
#elif defined(__linux__)
#define PALLET_CONSTANTS_PLATFORM PALLET_CONSTANTS_PLATFORM_LINUX
    constexpr platforms currentPlatform = platforms::Linux;
#else
#   error "Unsupported platform"
#endif
    
#ifdef RASPBERRYPI_PICO
#define PALLET_CONSTANTS_IS_EMBEDDED_DEVICE 1
    constexpr bool isEmbeddedDevice = true;
#else
    constexpr bool isEmbeddedDevice = false;
#endif
  }
}
