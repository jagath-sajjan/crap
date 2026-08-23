#include "audio.h"
#include "container.h"
#include <string.h>

int audio_write_frame(CrapContext *ctx, const int16_t *samples,
                      uint32_t sample_count, int64_t pts) {
    CrapFrameHeader fh;
    memset(&fh, 0, sizeof(fh));
    fh.stream_id  = 1;
    fh.frame_type = FRAME_I;
    fh.pts        = pts;
    fh.dts        = pts;
    fh.data_size  = sample_count * sizeof(int16_t);
    return crap_write_frame(ctx, &fh, (const uint8_t *)samples);
}

int audio_read_frame(CrapContext *ctx, int16_t *samples,
                     uint32_t max_samples, int64_t *pts_out) {
    for (;;) {
        CrapFrameHeader fh;
        int ret = crap_peek_frame(ctx, &fh);
        if (ret != CRAP_OK) return ret;

        if (fh.stream_id != 1) {
            ret = crap_skip_frame(ctx, &fh);
            if (ret != CRAP_OK) return ret;
            continue;
        }

        if (fh.data_size > max_samples * sizeof(int16_t))
            return CRAP_ERR_CORRUPT;
        ret = crap_read_frame_data(ctx, &fh,
                                   (uint8_t *)samples,
                                   max_samples * sizeof(int16_t));
        if (ret != CRAP_OK) return ret;
        if (pts_out) *pts_out = fh.pts;
        return (int)(fh.data_size / sizeof(int16_t));
    }
}
