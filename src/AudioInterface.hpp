#pragma once
#include "miniaudio.h"
#include <cmath>
#include <inttypes.h>
extern "C" {
#include "amy.h"
// #include "pcm_tiny.h"
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

void bleep() {
    struct event e = amy_default_event();
    int32_t start = amy_sysclock();   // Right now..
    e.time = start;
    e.osc = 0;
    e.wave = SINE;
    e.freq_coefs[COEF_CONST] = 220;
    e.velocity = 1;                   // start a 220 Hz sine.
    amy_add_event(e);
    e.time = start + 10;             // in 150 ms..
    e.freq_coefs[COEF_CONST] = 440;   // change to 440 Hz.
    amy_add_event(e);
    e.time = start + 20;          // in  300 ms..
    e.velocity = 0;                   // note off.
    amy_add_event(e);
}

void linux_audio_callback(ma_device* dev, void* inOut, const void* inIn, ma_uint32 nFrames) {
  float* out = reinterpret_cast<float*>(inOut);
  amy_prepare_buffer();
  amy_render(0, AMY_OSCS, 0);
  int16_t* samples = amy_fill_buffer();
  for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; i++) {
    out[i] = ((float)samples[i]) / 32767.0f;
  }
}



class LinuxAudioInterface : public AudioInterface {
  ma_device device;
public:
  void init() {
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
    config.playback.channels = 2;               // Set to 0 to use the device's native channel count.
    config.sampleRate        = 48000;           // Set to 0 to use the device's native sample rate.
    config.dataCallback      = &linux_audio_callback;   // This function will be called when miniaudio needs more data.
    config.pUserData         = nullptr;   // Can be accessed from the device object (device.pUserData)
    config.periodSizeInFrames = 64;
    
    // Phasor phasor(48000, 440);
    amy_start(1, 1, 0, 1);
    if (ma_device_init(nullptr, &config, &device) != MA_SUCCESS) {
      printf("Not successful!\n");
      return;
    }
    // bleep();
    ma_device_start(&device);
  }

  void cleanup() {
    ma_device_uninit(&device);
  }
};
