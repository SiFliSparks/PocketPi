#ifndef AUDIO_H
#define AUDIO_H

#include "rtthread.h"
#include "bf0_hal.h"
#include <drivers/audio.h>


#define RING_BUFFER_LENGTH 1536
#define DMA_BUF_SIZE    (256*2)
typedef struct audio_ring_buffer_s
{
    // TODO: 后续要考虑加锁保证线程安全
    int16_t samples[RING_BUFFER_LENGTH];
    int write_p, read_p;
} audio_ring_buffer_t;

int audio_ring_get_buffered();
int audio_ring_get_free();
void audio_ring_buffer_put(int16_t *samples, int length);
void audio_ring_buffer_get(int16_t *samples, int length);
rt_err_t audio_init();

#endif