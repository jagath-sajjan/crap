#include "decoder.h"
#include "entropy.h"
#include "dct.h"
#include "quant.h"
#include "colorspace.h"
#include "bitstream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int decoder_open(CrapDecoder *dec, const char *path, int quality) {
    memset(dec, 0, sizeof(*dec));
    int ret = crap_open_read(&dec->ctx, path);
    if (ret != CRAP_OK) return ret;
    CrapStreamInfo *si = NULL;
    for (int i = 0; i < CRAP_MAX_STREAMS; i++) {
        if (dec->ctx.streams[i].stream_type == STREAM_VIDEO) {
            si = &dec->ctx.streams[i]; break;
        }
    }
    if (!si) { crap_close(&dec->ctx); return CRAP_ERR_CORRUPT; }

    dec->width  = si->width;
    dec->height = si->height;
    quant_build_table(QUANT_LUMA_BASE,   quality, dec->qtable_luma);
    quant_build_table(QUANT_CHROMA_BASE, quality, dec->qtable_chroma);

    int ret2 = pool_init(&dec->frame_pool, sizeof(CrapFrame), DECODER_POOL_FRAMES);
    if (ret2 != CRAP_OK) { crap_close(&dec->ctx); return ret2; }
    return CRAP_OK;
}

static int decode_block(BSReader *r, uint8_t *dst, uint32_t stride,
                        const uint16_t qtable[64]) {
    int16_t block[64];
    if (entropy_decode_block(r, block) != 0) return -1;
    quant_decode(block, qtable);
    idct8x8(block);
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int v = (int)block[row*8+col] + 128;
            if (v <   0) v = 0;
            if (v > 255) v = 255;
            dst[row * stride + col] = (uint8_t)v;
        }
    }
    return 0;
}

CrapFrame *decoder_next_frame(CrapDecoder *dec) {
    uint32_t max_compressed = dec->width * dec->height * 3;
    uint8_t *compressed = malloc(max_compressed);
    if (!compressed) return NULL;

    CrapFrameHeader fh;
    for (;;) {
        int ret = crap_peek_frame(&dec->ctx, &fh);
        if (ret != CRAP_OK) { free(compressed); return NULL; }

        if (fh.stream_id >= CRAP_MAX_STREAMS ||
            dec->ctx.streams[fh.stream_id].stream_type != STREAM_VIDEO) {
            ret = crap_skip_frame(&dec->ctx, &fh);
            if (ret != CRAP_OK) { free(compressed); return NULL; }
            continue;
        }

        ret = crap_read_frame_data(&dec->ctx, &fh, compressed, max_compressed);
        if (ret != CRAP_OK) { free(compressed); return NULL; }
        break;
    }

    CrapFrame *f = (CrapFrame *)pool_acquire(&dec->frame_pool);

    uint8_t *rgb = malloc((size_t)dec->width * dec->height * 3);
    if (!rgb) { pool_release(&dec->frame_pool, f); free(compressed); return NULL; }

    frame_init(f);
    f->format = PIX_FMT_RGB24; f->width = dec->width; f->height = dec->height;
    f->pts = fh.pts; f->dts = fh.dts;
    f->frame_type      = (CrapFrameType)fh.frame_type;
    f->plane[0].data   = rgb;
    f->plane[0].width  = dec->width;
    f->plane[0].height = dec->height;
    f->plane[0].stride = dec->width * 3;

    size_t yuv_size = frame_buffer_size(PIX_FMT_YUV420P, dec->width, dec->height);
    uint8_t *yuv_buf = malloc(yuv_size);
    if (!yuv_buf) { free(rgb); pool_release(&dec->frame_pool, f); free(compressed); return NULL; }

    CrapFrame yf;
    frame_init(&yf);
    frame_attach_buffer(&yf, PIX_FMT_YUV420P, dec->width, dec->height, yuv_buf);

    BSReader r;
    bsr_init(&r, compressed, fh.data_size);

    uint32_t mb_w   = (dec->width    + 7) / 8;
    uint32_t mb_h   = (dec->height   + 7) / 8;
    uint32_t mb_w_c = (dec->width/2  + 7) / 8;
    uint32_t mb_h_c = (dec->height/2 + 7) / 8;


    uint8_t *yp  = yf.plane[0].data; uint32_t ys  = yf.plane[0].stride;
    uint8_t *cbp = yf.plane[1].data; uint32_t cbs = yf.plane[1].stride;
    uint8_t *crp = yf.plane[2].data; uint32_t crs = yf.plane[2].stride;

    uint32_t n = 0;
    for (uint32_t by = 0; by < mb_h; by++)
        for (uint32_t bx = 0; bx < mb_w; bx++, n++)
            if (decode_block(&r, yp+by*8*ys+bx*8, ys, dec->qtable_luma)) {
                goto fail;
            }
    bsr_align(&r);

    n = 0;
    for (uint32_t by = 0; by < mb_h_c; by++)
        for (uint32_t bx = 0; bx < mb_w_c; bx++, n++)
            if (decode_block(&r, cbp+by*8*cbs+bx*8, cbs, dec->qtable_chroma)) {
                goto fail;
            }
    bsr_align(&r);

    n = 0;
    for (uint32_t by = 0; by < mb_h_c; by++)
        for (uint32_t bx = 0; bx < mb_w_c; bx++, n++)
            if (decode_block(&r, crp+by*8*crs+bx*8, crs, dec->qtable_chroma)) {
                goto fail;
            }

    int ret = yuv_to_rgb24(&yf, rgb, dec->width, dec->height);
    free(yuv_buf); free(compressed);
    if (ret != 0) { free(rgb); pool_release(&dec->frame_pool, f); return NULL; }
    return f;

fail:
    free(yuv_buf); free(compressed); free(rgb);
    pool_release(&dec->frame_pool, f);
    return NULL;
}

void decoder_release_frame(CrapDecoder *dec, CrapFrame *f) {
    if (f->plane[0].data) free(f->plane[0].data);
    pool_release(&dec->frame_pool, f);
}

void decoder_close(CrapDecoder *dec) {
    crap_close(&dec->ctx);
    pool_destroy(&dec->frame_pool);
    memset(dec, 0, sizeof(*dec));
}
