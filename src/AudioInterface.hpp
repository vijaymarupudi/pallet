#include "RtAudio.h"
#include <cmath>
#include <inttypes.h>
extern "C" {
#include "amy.h"  
}


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
  float* out = reinterpret_cast<float*>(inOut);
  amy_prepare_buffer();
  amy_render(0, AMY_OSCS, 0);
  // printf("%d, %d\n", nFrames, );
  int16_t* samples = amy_fill_buffer();
  for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; i++) {
    // float samp = ((float)(rand())) / RAND_MAX * 2 - 1;
    // float samp = (((float)samples[i]) / INT16_MAX);
    // samp *= 0.05;
    out[i] = ((float)samples[i]) / 32767.0f * 0.1;
    // out[] = samp;
  }
  return 0;
}

void bleep() {
    struct event e = amy_default_event();
    int32_t start = amy_sysclock();   // Right now..
    e.time = start;
    e.osc = 0;
    e.wave = SINE;
    e.freq_coefs[COEF_CONST] = 220;
    e.velocity = 1;                   // start a 220 Hz sine.
    amy_add_event(e);
    e.time = start + 150;             // in 150 ms..
    e.freq_coefs[COEF_CONST] = 440;   // change to 440 Hz.
    amy_add_event(e);
    e.time = start + 300;          // in  300 ms..
    e.velocity = 0;                   // note off.
    amy_add_event(e);
}

class LinuxAudioInterface : public AudioInterface {
  RtAudio audio;
public:
  void init() {
    srand(time(NULL));
    unsigned int defDevice = audio.getDefaultOutputDevice();
    RtAudio::StreamParameters params {defDevice, 2, 0};
    unsigned int bufferFrames = 1024;
    // Phasor phasor(48000, 440);
    amy_start(1, 0, 0, 0);
    audio.openStream(&params, NULL, RTAUDIO_FLOAT32, 48000, &bufferFrames,
                     &linux_audio_callback, NULL, NULL);
    printf("Buffer: %u\n", bufferFrames);
    audio.startStream();
    bleep();
  }
};
