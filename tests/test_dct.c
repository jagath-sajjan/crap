#include "dct.h"
#include "dct.h"
#include <stdio.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

static inline int abs32(int x) { return x < 0 ? -x : x; }

static void test_dct_dc_only(void) {
    int16_t block[64];
    for (int i = 0; i < 64; i++) block[i] = 64;
    dct8x8(block);

    assert(block[0] != 0);
    for (int i = 1; i < 64; i++)
        assert(block[i] == 0);
    printf("[PASS] DCT flat block -> DCT only\n");
}

static void test_dct_idct_roundtrip(void) {
    int16_t orig[64], block[64];
    for (int i = 0; i < 64; i++) {
        orig[i] = (int16_t)((i * 13 + 7) % 265 - 128);
        block[i] = orig[i];
    }

    dct8x8(block);
    idct8x8(block);

    int max_err = 0;
    for (int i = 0; i < 64; i++) {
        int err = abs32((int)block[i] - (int)orig[i]);
        if (err > max_err) max_err = err;
    }
    printf("  DCT/IDCT max roundtrip error: %d\n", max_err);
    assert(max_err <= 8);
    printf("[PASS] DCT/IDCT roundtrip\n");
}

static void test_dct_zero_block(void) {
    int16_t block[64];
    memset(block, 0, sizeof(block));
    dct8x8(block);
    for (int i = 0; i < 64; i++) assert(block[i] == 0);
    idct8x8(block);
    for (int i = 0; i < 64; i++) assert(block[i] == 0);
    printf("[PASS] DCT zero block\n");
}

static void test_zigzag_bijection(void) {
    for (int i = 0; i < 64; i++)
        assert(IZIGZAG[ZIGZAG[i]] == (uint8_t)i);
    for (int i = 0; i < 64; i++)
        assert(ZIGZAG[IZIGZAG[i]] == (uint8_t)i);
    printf("[PASS] zigzag bijection\n");
}

static void test_dct_energy_compaction(void) {
    int16_t block[64];
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            block[r * 8 + c] = (int16_t)((r*3  + c*5 + 10) % 64 - 32);

        dct8x8(block);

        int64_t low_energy = 0, total_energy = 0;
        for (int i = 0; i < 64; i++) {
            int64_t v = (int64_t)block[ZIGZAG[i]] * block[ZIGZAG[i]];
            total_energy += v;
            if (i < 16) low_energy += v;
        }
        assert(total_energy > 0);
        assert(low_energy * 100 / total_energy >= 80);
        printf("[PASS] DCT energy compaction\n");
}

int main(void) {
    test_dct_dc_only();
    test_dct_idct_roundtrip();
    test_dct_zero_block();
    test_zigzag_bijection();
    test_dct_energy_compaction();
    printf("DCT tests passed yoo.\n");
    return 0;
}
