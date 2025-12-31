#ifndef AUDIO_H
#define AUDIO_H

#include "rtthread.h"
#include "bf0_hal.h"
#include <drivers/audio.h>
#include <string.h>

#define AUDIO_SAMPLE_RATE 32000
#define AUDIO_SAMPLE_BITS 16
#define AUDIO_CHANNEL_NUM    1

#define RING_BUFFER_LENGTH 1536
#define DMA_BUF_SIZE    (128*2)

// apu异步模拟的声音听起来有一点小问题，这种音频合成方式的代码暂时保留，后面再考虑改进
typedef struct audio_ring_buffer_s
{
    // TODO: 后续要考虑加锁保证线程安全
    int16_t samples[RING_BUFFER_LENGTH];
    int write_p, read_p;
} audio_ring_buffer_t;
extern int volume;

int audio_ring_get_buffered();
int audio_ring_get_free();
void audio_ring_buffer_put(int16_t *samples, int length);
void audio_ring_buffer_get(int16_t *samples, int length);
extern void audio_set_volume(int vol);
rt_err_t audio_init();

#endif