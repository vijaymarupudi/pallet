#include "RtAudio.h"
#include <cmath>

class AudioInterface {
  
};

struct Phasor {
  float msr;
  float mphase;
  float mincrement;
  float mfreq;

public:
  
  Phasor(float sr, float freq) {
    msr = sr;
    mfreq = freq;
    mphase = 0;
    mincrement = 0;
    calculateIncrement();
  }

  void calculateIncrement() {
    mincrement = mfreq / (float)msr;
  }

  void setFreq(float freq) {
    mfreq = freq;
    calculateIncrement();
  }

  float nextPhase() {
    float fin = fmod(mphase + mincrement, 1);
    return fin;
  }

  float getPhase() {
    return mphase;
  }

  float getIncrement() {
    return mincrement;
  }

  void increment() {
    mphase = fmod(mphase + mincrement, 1);
  }

};


int linux_audio_callback(void* inOut, void* in, unsigned int nFrames, double time, RtAudioStreamStatus status, void* udata) {
  Phasor* phasor = reinterpret_cast<Phasor*>(udata);
  float* out = reinterpret_cast<float*>(inOut);
  for (int i = 0; i < nFrames / 2; i++) {
    float samp = phasor->getPhase() * 2 - 1;
    phasor->nextPhase();
    samp *= 0.05;
    out[i * 2] = samp;
    out[i * 2 + 1] = -1 * samp;
  }
  return 0;
}

class LinuxAudioInterface : public AudioInterface {
  RtAudio audio;
public:
  void init() {
    srand(time(NULL));
    unsigned int defDevice = audio.getDefaultOutputDevice();
    RtAudio::StreamParameters params {defDevice, 2, 0};
    unsigned int bufferFrames = 1024;
    Phasor phasor(48000, 440);
    audio.openStream(&params, NULL, RTAUDIO_FLOAT32, 48000, &bufferFrames,
                     &linux_audio_callback, &phasor, NULL);
    audio.startStream();
  }
};
