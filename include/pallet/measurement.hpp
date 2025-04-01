#pragma once

namespace pallet {
template <class T, size_t maxLen>
struct RunningMeanMeasurer {
  size_t len = 0;
  size_t index = 0;
  T avg = 0;
  T samples[maxLen];

  void addSample(T sample) {
    if (len < maxLen) {
      samples[len] = sample;
      len++;
      index++;
      avg = avg * (len - 1) / len + sample / len;
    } else {
      index = (index + 1) % maxLen;
      auto oldSample = samples[index];
      samples[index] = sample;
      avg = avg + (-oldSample + sample) / len;
    }
  }

  T mean() const {
    return avg;
  }

  void clear() {
    len = 0;
    index = 0;
    avg = 0;
  }
};

}
