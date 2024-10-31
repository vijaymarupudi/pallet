#include "miniaudio.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void data_callback(ma_device* pDevice, void* inOut, const void* pInput, ma_uint32 frameCount)
{
    // return;
  float* out = reinterpret_cast<float*>(inOut);
  // amy_prepare_buffer();
  // amy_render(0, AMY_OSCS, 0);
  // // printf("%d, %d\n", nFrames, );
  // int16_t* samples = amy_fill_buffer();
  // for (int i = 0; i < AMY_BLOCK_SIZE * AMY_NCHANS; i++) {
  //   out[i] = ((float)samples[i]) / 32767.0f;
  // }
  for (int i = 0; i < frameCount * 2; i++) {
    out[i] = ((float)rand()) / RAND_MAX * 0.1;
  }
 
    // In playback mode copy data to pOutput. In capture mode read data from pInput. In full-duplex mode, both
    // pOutput and pInput will be valid and you can move data from pInput into pOutput. Never process more than
    // frameCount frames.
}

int main()
{
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;   // Set to ma_format_unknown to use the device's native format.
    config.playback.channels = 2;               // Set to 0 to use the device's native channel count.
    config.sampleRate        = 48000;           // Set to 0 to use the device's native sample rate.
    config.dataCallback      = data_callback;   // This function will be called when miniaudio needs more data.
    config.pUserData         = nullptr;   // Can be accessed from the device object (device.pUserData).

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
        return -1;  // Failed to initialize the device.
    }

    ma_device_start(&device);     // The device is sleeping by default so you'll need to start it manually.

    // Do something here. Probably your program's main loop.

    sleep(5);

    ma_device_uninit(&device);
    return 0;
}
