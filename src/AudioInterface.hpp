#pragma once
#include "miniaudio.h"
#include "constants.hpp"
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

#if PALLET_CONSTANTS_PLATFORM == PALLET_CONSTANTS_PLATFORM_LINUX

#include <atomic>

static void linuxAudioInterfaceCallback(ma_device* dev, void* inOut, const void* inIn, ma_uint32 nFrames);


class LinuxAudioInterface : public AudioInterface {
public:
  Clock* clock;
  ma_device device;
  ma_encoder encoder;
  ma_pcm_rb recordingRingBuffer;
  std::atomic<bool> recording = false;
  Clock::id_type recordingIntervalId;

  void init(Clock* clock) {
    this->clock = clock;

    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
    config.playback.channels = 2;               // Set to 0 to use the device's native channel count.
    config.sampleRate        = 48000;           // Set to 0 to use the device's native sample rate.
    config.dataCallback      = &linuxAudioInterfaceCallback;   // This function will be called when miniaudio needs more data.
    config.pUserData         = this;   // Can be accessed from the device object (device.pUserData)
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

  void toggleRecord() {
    if (!recording) {
      ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, 2, 48000);
      ma_encoder_init_file("out.wav", &config, &encoder);
      ma_pcm_rb_init(ma_format_f32, 2, 48000, NULL, NULL, &recordingRingBuffer);
      this->recording = true;
      this->recordingIntervalId = this->clock->setInterval(1000 * 100, [](void* ud) {
        auto iface = (LinuxAudioInterface*)ud;
        if (!iface->recording) { return; }
        float* buf;
        ma_uint32 nFrames = 8192;
        auto res = ma_pcm_rb_acquire_read(&iface->recordingRingBuffer, &nFrames, (void**)&buf);
        if (nFrames != 0) {
          ma_encoder_write_pcm_frames(&iface->encoder, buf, nFrames, nullptr);
        }
        ma_pcm_rb_commit_read(&iface->recordingRingBuffer, nFrames);
      }, this);
    } else {
      this->recording = false;
      this->clock->clearInterval(this->recordingIntervalId);
      ma_encoder_uninit(&encoder);
      ma_pcm_rb_uninit(&recordingRingBuffer);
    }
  }
  void cleanup() {
    if (this->recording) {
      this->toggleRecord();
    }
    ma_device_uninit(&device);
  }

  void audioThreadFunc(void* inOut, ma_uint32 nFrames) {
    float* out = reinterpret_cast<float*>(inOut);
    amy_prepare_buffer();
    amy_render(0, AMY_OSCS, 0);
    int16_t* samples = amy_fill_buffer();
    for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; i++) {
      out[i] = ((float)samples[i]) / 32767.0f;
    }
    if (this->recording) {
      ma_uint32 nFrames = AMY_BLOCK_SIZE;
      float* buf;
      auto res = ma_pcm_rb_acquire_write(&this->recordingRingBuffer, &nFrames, (void**)&buf);
      if (res != MA_SUCCESS) { return; }
      memcpy(buf, out, sizeof(float) * 2 * nFrames);
      ma_pcm_rb_commit_write(&this->recordingRingBuffer, nFrames);
    }
  }
};

static void linuxAudioInterfaceCallback(ma_device* dev, void* inOut, const void* inIn, ma_uint32 nFrames) {
  ((LinuxAudioInterface*)dev->pUserData)->audioThreadFunc(inOut, nFrames);
}

#endif
