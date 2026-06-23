#include "encoder.h"
#include "colorspace.h"
#include "dct.h"
#include "quant.h"
#include "entropy.h"
#include "bitstream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int encoder_open(CrapEncoder *enc, const char *path,
                 uint32_t width, uint32_t height,
                 uint8_t fps_num, uint8_t fps_den,
                 int quality) {
    memset(enc, 0, sizeof(*enc));
    enc->quality   = quality;
    enc->stream_id = 0;

    quant_build_table(QUANT_LUMA_BASE,   quality, enc->qtable_luma);
    quant_build_table(QUANT_CHROMA_BASE, quality, enc->qtable_chroma);

    int ret = crap_open_write(&enc->ctx, path);
    if (ret != CRAP_OK) return ret;

    CrapStreamInfo si;
    memset(&si, 0, sizeof(si));
    si.stream_id     = 0;
    si.stream_type   = STREAM_VIDEO;
    si.codec_id      = 0x0001;
    si.width         = (uint16_t)width;
    si.height        = (uint16_t)height;
    si.fps_num       = fps_num;
    si.fps_den       = fps_den;
    si.time_base_num = 1;
    si.time_base_den = 1000000;
    enc->ctx.streams[0]          = si;
    enc->ctx.header.stream_count = 1;

    uint32_t sinf_id  = CHUNK_STREAMINFO;
    uint64_t sinf_sz  = sizeof(CrapStreamInfo);
    uint32_t sinf_crc = crap_crc32((const uint8_t *)&si, sizeof(si));
    fwrite(&sinf_id,  4,          1, enc->ctx.fp);
    fwrite(&sinf_sz,  8,          1, enc->ctx.fp);
    fwrite(&si,       sizeof(si), 1, enc->ctx.fp);
    fwrite(&sinf_crc, 4,          1, enc->ctx.fp);

    return CRAP_OK;
}

static int encode_block(const uint8_t *src, uint32_t stride,
                        const uint16_t qtable[64], BSWriter *w) {
    int16_t block[64];
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            block[r*8+c] = (int16_t)src[r * stride + c] - 128;
    dct8x8(block);
    quant_encode(block, qtable);
    return entropy_encode_block(w, block);
}

int encoder_write_iframe_rgb(CrapEncoder *enc,
                             const uint8_t *rgb,
                             uint32_t width, uint32_t height,
                             int64_t pts) {
    size_t fbuf_size = frame_buffer_size(PIX_FMT_YUV420P, width, height);
    uint8_t *fbuf = malloc(fbuf_size);
    if (!fbuf) return CRAP_ERR_OOM;

    CrapFrame f;
    frame_init(&f);
    frame_attach_buffer(&f, PIX_FMT_YUV420P, width, height, fbuf);

    int ret = rgb24_to_yuv(rgb, &f, width, height);
    if (ret != 0) { free(fbuf); return ret; }

    uint32_t mb_w   = (width    + 7) / 8;
    uint32_t mb_h   = (height   + 7) / 8;
    uint32_t mb_w_c = (width/2  + 7) / 8;
    uint32_t mb_h_c = (height/2 + 7) / 8;
    size_t bs_size  = (size_t)(mb_w * mb_h + 2 * mb_w_c * mb_h_c) * 256;
    uint8_t *bsbuf  = malloc(bs_size);
    if (!bsbuf) { free(fbuf); return CRAP_ERR_OOM; }

    BSWriter w;
    bsw_init(&w, bsbuf, bs_size);

    uint8_t *yp = f.plane[0].data;
    uint32_t ys = f.plane[0].stride;
    for (uint32_t by = 0; by < mb_h; by++) {
        for (uint32_t bx = 0; bx < mb_w; bx++) {
            ret = encode_block(yp + by*8*ys + bx*8, ys,
                               enc->qtable_luma, &w);
            if (ret != 0) { free(fbuf); free(bsbuf); return ret; }
        }
    }

    uint8_t *cbp = f.plane[1].data;
    uint32_t cbs = f.plane[1].stride;
    for (uint32_t by = 0; by < mb_h_c; by++) {
        for (uint32_t bx = 0; bx < mb_w_c; bx++) {
            bsw_align(&w);
    ret = encode_block(cbp + by*8*cbs + bx*8, cbs,
                               enc->qtable_chroma, &w);
            if (ret != 0) { free(fbuf); free(bsbuf); return ret; }
        }
    }

    uint8_t *crp = f.plane[2].data;
    uint32_t crs = f.plane[2].stride;
    for (uint32_t by = 0; by < mb_h_c; by++) {
        for (uint32_t bx = 0; bx < mb_w_c; bx++) {
            bsw_align(&w);
    ret = encode_block(crp + by*8*crs + bx*8, crs,
                               enc->qtable_chroma, &w);
            if (ret != 0) { free(fbuf); free(bsbuf); return ret; }
        }
    }

    bsw_align(&w);
    size_t payload_bytes = bsw_bytes_written(&w);

    CrapFrameHeader fh;
    memset(&fh, 0, sizeof(fh));
    fh.stream_id  = enc->stream_id;
    fh.frame_type = FRAME_I;
    fh.pts        = pts;
    fh.dts        = pts;
    fh.data_size  = (uint32_t)payload_bytes;

    ret = crap_write_frame(&enc->ctx, &fh, bsbuf);
    enc->frame_index++;
    free(fbuf);
    free(bsbuf);
    return ret;
}

int encoder_close(CrapEncoder *enc) {
    enc->ctx.header.stream_count = 1;
    enc->ctx.header.header_crc32 = crap_crc32(
        (const uint8_t *)&enc->ctx.header,
        sizeof(CrapFileHeader) - sizeof(uint32_t));
    rewind(enc->ctx.fp);
    fwrite(&enc->ctx.header, sizeof(CrapFileHeader), 1, enc->ctx.fp);
    crap_close(&enc->ctx);
    return CRAP_OK;
}
