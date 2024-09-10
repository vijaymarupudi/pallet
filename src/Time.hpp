#include <inttypes.h>
#include <time.h>


class TimeInterface {
  virtual void init() {};
  virtual uint64_t current_time() = 0;
};

class LinuxTimeInterface : public TimeInterface {
  
};



struct timespec get_current_time() {
  struct timespec spec;
  clock_gettime(CLOCK_MONOTONIC, &spec);
  return spec;
}

// struct timespec timespec_add(struct timespec one, struct timespec two) {

// }

struct timespec timespec_add(struct timespec one, unsigned long nsec) {
  struct timespec out = {one.tv_sec, one.tv_nsec};
  if ((one.tv_nsec + nsec) < 1000000000) {
    out.tv_nsec = one.tv_nsec + nsec;
  } else {
    out.tv_nsec = one.tv_nsec + nsec - 1000000000;
    out.tv_sec += 1;
  }
  return out;
}

class IntervalTimer {
public:
  unsigned long interval;
  int timerfd = -1;
  struct timespec prev_time;
  IntervalTimer(unsigned long interval) : interval(interval) {
    this->timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
  }

  void begin() {
    this->prev_time = get_current_time();
  }

  struct timespec next() {
    auto goal_time = timespec_add(this->prev_time, this->interval);
    struct itimerspec spec = {0};
    spec.it_value = goal_time;
    timerfd_settime(this->timerfd, TFD_TIMER_ABSTIME, &spec, NULL);
    uint64_t expirations;
    read(this->timerfd, &expirations, sizeof(uint64_t));
    this->prev_time = goal_time;
    return goal_time;
  }

  ~IntervalTimer() {
    close(this->timerfd);
  }

};
