#include "encoder.h"
#include "decoder.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define W 64
#define H 64

static inline int absi(int x) { return x < 0 ? - x : x ; }

int main(void) {
    const char *path = "/tmp/test_e2e.crap";

    uint8_t *rgb_in = malloc(W * H * 3);
    assert(rgb_in);
    for (int i = 0; i < W * H; i++) {
        rgb_in[i*3+0] = (uint8_t)(i % 256);
        rgb_in[i*3+1] = (uint8_t)((i * 3) % 256);
        rgb_in[i*3+2] = (uint8_t)((i * 7) % 256);
    }

    CrapEncoder enc;
    int ret = encoder_open(&enc, path, W, H, 30, 1, 90);
    assert(ret == CRAP_OK);

    ret = encoder_write_iframe_rgb(&enc, rgb_in, W, H, 0);
    assert(ret == CRAP_OK);

    ret = encoder_close(&enc);
    assert(ret == CRAP_OK);
    printf("[PASS] encode\n");

    CrapDecoder dec;
    ret = decoder_open(&dec, path, 90);
    assert(ret == CRAP_OK);

    CrapFrame *f = decoder_next_frame(&dec);
    assert(f != NULL);
    assert(f -> width == W);
    assert(f -> height == H);
    assert(f -> frame_type == FRAME_I);

    uint8_t *rgb_out = f -> plane[0].data;
    int max_err = 0;
    for (int i = 0; i < W * H * 3; i++) {
        int e = absi((int)rgb_out[i] - (int)rgb_in[i]);
        if (e > max_err) max_err = e;
    }
    printf(" e2e max pixel error (q90): %d\n", max_err);
    assert(max_err <= 250);

    decoder_release_frame(&dec, f);
    decoder_close(&dec);
    printf("[PASS] decode\n");
    printf("END-2-END iframe test passed\n");

    free(rgb_in);
    return 0;
}
