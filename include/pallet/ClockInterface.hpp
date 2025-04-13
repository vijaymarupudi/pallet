#pragma once

#include <cstdint>
#include <type_traits>

#include "pallet/constants.hpp"
#include "pallet/functional.hpp"
#include "pallet/time.hpp"

namespace pallet {

class ClockInterface {
public:
  using Id = std::conditional_t<pallet::constants::isEmbeddedDevice, uint8_t, uint32_t>;
  
  struct EventInfo {
    Id id;
    pallet::Time now;
    pallet::Time intended;
    pallet::Time period;
    size_t overhead;
  };

  using Callback = Callable<void(const EventInfo&)>;

  virtual pallet::Time currentTime() = 0;
  virtual Id setTimeout(pallet::Time duration,
                        Callback callback) = 0;
  virtual Id setTimeoutAbsolute(pallet::Time goal,
                                Callback callback) = 0;
  virtual Id setInterval(pallet::Time period,
                         Callback callback) = 0;
  virtual Id setIntervalAbsolute(pallet::Time goal,
                                 pallet::Time period,
                                 Callback callback) = 0;
  virtual void clearTimeout(Id id) = 0;
  virtual void clearInterval(Id id) = 0;
  virtual ~ClockInterface() {};
};

}


