#include "colorspace.h"
#include "frame.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define W 64
#define H 64

static void test_yuv444_roundtrip(void) {
    uint8_t *rgb_in  = malloc(W * H * 3);
    uint8_t *rgb_out = malloc(W * H * 3);
    uint8_t *fbuf    = malloc(frame_buffer_size(PIX_FMT_YUV444P, W, H));
    assert(rgb_in && rgb_out && fbuf);

    for (uint32_t i = 0; i < W * H; i++) {
        rgb_in[i*3+0] = (uint8_t)(i % 256);
        rgb_in[i*3+1] = (uint8_t)((i * 3) % 256);
        rgb_in[i*3+2] = (uint8_t)((i * 7) % 256);
    }

    CrapFrame f;
    frame_init(&f);
    frame_attach_buffer(&f, PIX_FMT_YUV444P, W, H, fbuf);
    assert(rgb24_to_yuv(rgb_in, &f, W, H) == 0);
    assert(yuv_to_rgb24(&f, rgb_out, W, H) == 0);

    int max_err = 0;
    for (uint32_t i = 0; i < W * H * 3; i++) {
        int err = abs((int)rgb_out[i] - (int)rgb_in[i]);
        if (err > max_err) max_err = err;
    }
    printf("  YUV444 max roundtrip error: %d\n", max_err);
    assert(max_err <= 2);

    free(rgb_in); free(rgb_out); free(fbuf);
    printf("[PASS] YUV444P roundtrip\n");
}

static void test_yuv420_chroma_dims(void) {
    uint8_t *rgb  = malloc(W * H * 3);
    uint8_t *fbuf = malloc(frame_buffer_size(PIX_FMT_YUV420P, W, H));
    assert(rgb && fbuf);
    memset(rgb, 0x80, W * H * 3);
    CrapFrame f;
    frame_init(&f);
    frame_attach_buffer(&f, PIX_FMT_YUV420P, W, H, fbuf);
    assert(rgb24_to_yuv(rgb, &f, W, H) == 0);
    assert(f.plane[1].width == W/2 && f.plane[1].height == H/2);
    assert(f.plane[2].width == W/2 && f.plane[2].height == H/2);
    free(rgb); free(fbuf);
    printf("[PASS] YUV420P chroma dims\n");
}

static void test_yuv422_chroma_dims(void) {
    uint8_t *rgb  = malloc(W * H * 3);
    uint8_t *fbuf = malloc(frame_buffer_size(PIX_FMT_YUV422P, W, H));
    assert(rgb && fbuf);
    memset(rgb, 0x80, W * H * 3);
    CrapFrame f;
    frame_init(&f);
    frame_attach_buffer(&f, PIX_FMT_YUV422P, W, H, fbuf);
    assert(rgb24_to_yuv(rgb, &f, W, H) == 0);
    assert(f.plane[1].width == W/2 && f.plane[1].height == H);
    assert(f.plane[2].width == W/2 && f.plane[2].height == H);
    free(rgb); free(fbuf);
    printf("[PASS] YUV422P chroma dims\n");
}

static void test_pure_colors(void) {
    struct { uint8_t r,g,b; uint8_t ey,ecb,ecr; } cases[] = {
        { 255,   0,   0,  76,  85, 255 },
        {   0, 255,   0, 150,  44,  21 },
        {   0,   0, 255,  29, 255, 107 },
        { 255, 255, 255, 255, 128, 128 },
        {   0,   0,   0,   0, 128, 128 },
    };
    int n = (int)(sizeof(cases)/sizeof(cases[0]));
    for (int i = 0; i < n; i++) {
        uint8_t rgb[3] = { cases[i].r, cases[i].g, cases[i].b };
        uint8_t fbuf[frame_buffer_size(PIX_FMT_YUV444P, 1, 1)];
        CrapFrame f;
        frame_init(&f);
        frame_attach_buffer(&f, PIX_FMT_YUV444P, 1, 1, fbuf);
        assert(rgb24_to_yuv(rgb, &f, 1, 1) == 0);
        int ey  = abs((int)f.plane[0].data[0] - (int)cases[i].ey);
        int ecb = abs((int)f.plane[1].data[0] - (int)cases[i].ecb);
        int ecr = abs((int)f.plane[2].data[0] - (int)cases[i].ecr);
        assert(ey <= 2 && ecb <= 2 && ecr <= 2);
    }
    printf("[PASS] pure color BT.601 full-swing values\n");
}

static void test_rgba_alpha_ignored(void) {
    uint8_t rgba[4] = { 200, 100, 50, 0 };
    uint8_t rgb[3]  = { 200, 100, 50 };
    uint8_t fa_buf[frame_buffer_size(PIX_FMT_YUV444P, 1, 1)];
    uint8_t fb_buf[frame_buffer_size(PIX_FMT_YUV444P, 1, 1)];
    CrapFrame fa, fb;
    frame_init(&fa); frame_init(&fb);
    frame_attach_buffer(&fa, PIX_FMT_YUV444P, 1, 1, fa_buf);
    frame_attach_buffer(&fb, PIX_FMT_YUV444P, 1, 1, fb_buf);
    assert(rgba32_to_yuv(rgba, &fa, 1, 1) == 0);
    assert(rgb24_to_yuv(rgb,   &fb, 1, 1) == 0);
    assert(fa.plane[0].data[0] == fb.plane[0].data[0]);
    assert(fa.plane[1].data[0] == fb.plane[1].data[0]);
    assert(fa.plane[2].data[0] == fb.plane[2].data[0]);
    printf("[PASS] RGBA alpha ignored\n");
}

int main(void) {
    test_yuv444_roundtrip();
    test_yuv420_chroma_dims();
    test_yuv422_chroma_dims();
    test_pure_colors();
    test_rgba_alpha_ignored();
    printf("Colorspace tests passed.\n");
    return 0;
}
