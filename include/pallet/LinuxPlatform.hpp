#include "PosixPlatform.hpp"

namespace pallet {
  class LinuxPlatform : public PosixPlatform {
  public:
    LinuxPlatform();
    ~LinuxPlatform();
  private:
    int cpu_dma_latency_fd;
  };
}
