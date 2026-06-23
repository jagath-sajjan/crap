#include "quant.h"
#include <stdint.h>

const uint8_t QUANT_LUMA_BASE[64] = {
    16, 11, 10, 16,  24,  40,  51,  61,
    12, 12, 14, 19,  26,  58,  60,  55,
    14, 13, 16, 24,  40,  57,  69,  56,
    14, 17, 22, 29,  51,  87,  80,  62,
    18, 22, 37, 56,  68, 109, 103,  77,
    24, 35, 55, 64,  81, 104, 113,  92,
    49, 64, 78, 87, 103, 121, 120, 101,
    72, 92, 95, 98, 112, 100, 103,  99,
};

const uint8_t QUANT_CHROMA_BASE[64] = {
    17, 18, 24, 47, 99, 99, 99, 99,
    18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99,
    47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99,
};

void quant_build_table(const uint8_t base[64], int quality, uint16_t out[64]) {
    if (quality < 1)   quality = 1;
    if (quality > 100) quality = 100;

    int scale = (quality < 50)
        ? (5000 / quality)
        : (200 - 2 * quality);

    for (int i = 0; i < 64; i++) {
        int v = ((int)base[i] * scale + 50) / 100;
        if (v < 1) v = 1;
        if (v > 255) v = 255;
       out[i] = (uint16_t)v;
    }
}

void quant_encode(int16_t block[64], const uint16_t qtable[64]) {
    int16_t tmp[64];
    for (int i = 0; i < 64; i++) {
        int zi = ZIGZAG[i];
        int val = block[zi];
        int q = qtable[i];
        if (val >= 0)
            tmp[i] = (int16_t)((val + q / 2) / q);
        else
            tmp[i] = (int16_t)(-( (val) + q / 2) / q);
    }
    for (int i = 0; i < 64; i++) block[i] = tmp[i];
}

void quant_decode(int16_t block[64], const uint16_t qtable[64]) {
    int16_t tmp[64];
    for (int i = 0; i < 64; i++)
        tmp[ZIGZAG[i]] = (int16_t)((int)block[i] * (int)qtable[i]);
    for (int i = 0; i < 64; i++) block[i] = tmp[i];
}
