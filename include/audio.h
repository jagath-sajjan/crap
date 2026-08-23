#ifndef AUDIO_H
#define AUDIO_H

#include "container.h"
#include <stdint.h>
#include <stddef.h>

/*
 * audio stream params ~ PCM 16 bit signed interleaved stereo 
 */
#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_CHANNELS    2
#define AUDIO_BITS        16
#define AUDIO_FRAME_SAMPLES 1024 // samples per channel per chunk

/*
 * write 1 audio chunk into container
 */
int audio_write_frame(CrapContext *ctx, const int16_t *samples, uint32_t sample_count, int64_t pts);

/*
 * read 1 audio chunk from container into samples 
 */
int audio_read_frame(CrapContext *ctx, int16_t *samples, uint32_t max_samples, int64_t *pts_out);

#endif /* AUDIO_H */
