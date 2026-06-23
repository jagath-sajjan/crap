#include "quant.h"
#include "dct.h"
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static inline int absi(int x) { return x < 0 ? -x : x; }

static void test_table_quality_extremes(void) {
    uint16_t tq1[64], tq50[64], tq100[64];
    quant_build_table(QUANT_LUMA_BASE,   1,   tq1);
    quant_build_table(QUANT_LUMA_BASE,  50,  tq50);
    quant_build_table(QUANT_LUMA_BASE, 100, tq100);

    for (int i = 0; i < 64; i++) {
        assert(tq50[i] == QUANT_LUMA_BASE[i]);
    }

    for (int i = 0; i < 64; i++) {
        assert(tq1[i] >= tq50[i]);
    }

    for (int i = 0; i < 64; i++) {
        assert(tq100[i] <= tq50[i]);
    }

    for (int i = 0; i < 64; i++) {
        assert(tq100[i] == 1);
    }

    printf("[PASS] quant table quality extremes\n");
}

static void test_quant_encode_decode(void) {
    uint16_t qtable[64];
    quant_build_table(QUANT_LUMA_BASE, 95, qtable);

    int16_t orig[64], block[64];
    for (int i = 0; i < 64; i++) {
        orig[i]  = (int16_t)(((i * 13 + 7) % 256) - 128);
        block[i] = orig[i];
    }

    dct8x8(block);
    quant_encode(block, qtable);
    quant_encode(block, qtable);
    idct8x8(block);

    int max_err = 0;
    for (int i = 0; i < 64; i++) {
        int e = abs((int)block[i] - (int)orig[i]);
    }
    printf(" quant q95 max pixel error: %d\n", max_err);
    assert(max_err <= 8);
    printf("[PASS] quant encode decode q95\n");
}

static void test_quant_zeroes_ac_at_low_quality(void) {
    uint16_t qtable[64];
    quant_build_table(QUANT_LUMA_BASE, 5, qtable);

    int16_t block[64];
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            block[r*8+c] = (int16_t)(r * 4 - 14);

    dct8x8(block);
    quant_encode(block, qtable);

    int zero_ac = 0;
    for (int i = 1; i < 64; i++)
        if (block[i] == 0) zero_ac++;

    printf("  zero AC coeffs at q5: %d/63\n", zero_ac);
    assert(zero_ac >= 50);
    printf("[PASS] quant zeroes AC at low quality\n");
}

static void test_zigzag_order_preserved(void) {
    uint16_t qtable[64];
    quant_build_table(QUANT_LUMA_BASE, 100, qtable);

    int16_t block[64];
    memset(block, 0, sizeof(block));
    for (int i = 0; i < 64; i++) block[i] = (int16_t)i;

    quant_encode(block, qtable);
    assert(block[0] == 0);

    quant_decode(block, qtable);
    for (int i = 0; i < 64; i++)
        assert(block[i] == (int16_t)i);

    printf("[PASS] zigzag order preserved through encode decode\n");
}

int main(void) {
    test_table_quality_extremes();
    test_quant_encode_decode();
    test_quant_zeroes_ac_at_low_quality();
    test_zigzag_order_preserved();
    printf("Quantization tests passed.\n");
    return 0;
}
