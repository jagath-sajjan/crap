#include "container.h"
#include "audio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(void) {
    const char *test_path = "/tmp/test_container.crap";

    CrapContext ctx;
    int ret = crap_open_write(&ctx, test_path);
    assert(ret == CRAP_OK);

    CrapStreamInfo vsi;
    memset(&vsi, 0, sizeof(vsi));
    vsi.stream_id = 0;
    vsi.stream_type = STREAM_VIDEO;
    vsi.codec_id = 0x0001;
    vsi.width = 640;
    vsi.height = 480;
    vsi.fps_num = 30;
    vsi.fps_den = 1;
    vsi.time_base_num = 1;
    vsi.time_base_den = 1000000;
    ret = crap_write_streaminfo(&ctx, &vsi);
    assert(ret == CRAP_OK);

    CrapStreamInfo asi;
    memset(&asi, 0, sizeof(asi));
    asi.stream_id = 1;
    asi.stream_type = STREAM_AUDIO;
    asi.codec_id = 0x0002;
    asi.sample_rate = 44100;
    asi.channels = 2;
    asi.bit_depth = 16;
    asi.time_base_num = 1;
    asi.time_base_den = 1000000;
    ret = crap_write_streaminfo(&ctx, &asi);
    assert(ret == CRAP_OK);

    uint8_t dummy_vdata[256];
    memset(dummy_vdata, 0xAB, sizeof(dummy_vdata));

    CrapFrameHeader fh;
    memset(&fh, 0, sizeof(fh));
    fh.stream_id = 0;
    fh.frame_type = FRAME_I;
    fh.pts = 0;
    fh.dts = 0;
    fh.data_size = sizeof(dummy_vdata);
    ret = crap_write_frame(&ctx, &fh, dummy_vdata);
    assert(ret == CRAP_OK);

    int16_t dummy_adata[512];
    for (int i = 0; i < 512; i++) dummy_adata[i] = (int16_t)(i * 10);
    ret = audio_write_frame(&ctx, dummy_adata, 512, 10000);
    assert(ret == CRAP_OK);

    fh.pts = 33333;
    fh.dts = 33333;
    fh.frame_type = FRAME_P;
    ret = crap_write_frame(&ctx, &fh, dummy_vdata);
    assert(ret == CRAP_OK);

    ret = crap_write_index(&ctx);
    assert(ret == CRAP_OK);

    ctx.header.duration_us = ctx.last_pts;
    ctx.header.header_crc32 = crap_crc32((const uint8_t *)&ctx.header, sizeof(CrapFileHeader) - sizeof(uint32_t));
    rewind(ctx.fp);
    fwrite(&ctx.header, sizeof(CrapFileHeader), 1, ctx.fp);
    crap_close(&ctx);
    printf("[PASS] container write\n");

    // Read back and verify
    CrapContext rctx;
    ret = crap_open_read(&rctx, test_path);
    assert(ret == CRAP_OK);
    assert(rctx.header.magic == CRAP_MAGIC);
    assert(rctx.header.stream_count == 2);
    assert(rctx.streams[0].stream_type == STREAM_VIDEO);
    assert(rctx.streams[0].width == 640);
    assert(rctx.streams[0].height == 480);
    assert(rctx.streams[1].stream_type == STREAM_AUDIO);
    assert(rctx.streams[1].sample_rate == 44100);
    assert(rctx.index_count == 3);

    uint8_t read_buf[1024];
    CrapFrameHeader rfh;
    ret = crap_read_frame(&rctx, &rfh, read_buf, sizeof(read_buf));
    assert(ret == CRAP_OK);
    assert(rfh.stream_id == 0);
    assert(rfh.frame_type == FRAME_I);
    assert(rfh.pts == 0);
    assert(rfh.data_size == sizeof(dummy_vdata));
    assert(memcmp(read_buf, dummy_vdata, sizeof(dummy_vdata)) == 0);

    ret = crap_seek_pts(&rctx, 0, 0);
    assert(ret == CRAP_OK);
    ret = crap_read_frame(&rctx, &rfh, read_buf, sizeof(read_buf));
    assert(ret == CRAP_OK);
    assert(rfh.stream_id == 0);
    assert(rfh.pts == 0);

    crap_close(&rctx);
    printf("[PASS] container read and seek\n");
    printf("Container tests passed.\n");
    return 0;
}
