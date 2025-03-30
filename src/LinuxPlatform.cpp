#include "constants.hpp"

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

#include <unistd.h>
#include <sched.h>
#include <sys/resource.h>
#include <cstring>
#include <fcntl.h>

#include "LinuxPlatform.hpp"

namespace pallet {

static void setThreadToHighPriority() {
  // increase the niceness of the process
  int ret = getpriority(PRIO_PROCESS, 0);
  setpriority(PRIO_PROCESS, 0, ret - 4);

  // enable SCHED_FIFO for realtime scheduling without preemption
  // new threads can't inherit realtime status
  struct sched_param schparam;
  memset(&schparam, 0, sizeof(struct sched_param));
  schparam.sched_priority = 60;
  if (sched_setscheduler(0, SCHED_FIFO | SCHED_RESET_ON_FORK,
                         &schparam) == 0) {
    fprintf(stderr, "realtime scheduling enabled\n");
  }
}

LinuxPlatform::LinuxPlatform() : PosixPlatform() {
  
  this->cpu_dma_latency_fd = open("/dev/cpu_dma_latency", O_WRONLY);
  if (this->cpu_dma_latency_fd >= 0) {
    int val = 0;
    write(this->cpu_dma_latency_fd, &val, sizeof(val));
  }

  setThreadToHighPriority();

}

LinuxPlatform::~LinuxPlatform() {
  if (this->cpu_dma_latency_fd >= 0) {
    close(this->cpu_dma_latency_fd);
  }
}

}

#endif
