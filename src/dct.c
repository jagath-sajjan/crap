#include "dct.h"
#include <stdint.h>

static const int32_t COS_TABLE[8][8] = {
    { 1024, 1004,  946,  851,  724,  569,  392,  200 },
    { 1024,  851,  392, -200, -724, -1004, -946, -569 },
    { 1024,  569, -392, -1004, -724,  200,  946,  851 },
    { 1024,  200, -946, -569,  724,  851, -392, -1004 },
    { 1024, -200, -946,  569,  724, -851, -392,  1004 },
    { 1024, -569, -392,  1004, -724, -200,  946, -851 },
    { 1024, -851,  392,  200, -724,  1004, -946,  569 },
    { 1024, -1004,  946, -851,  724, -569,  392, -200 },
};

static void dct1d(int32_t *v) {
    int32_t out[8];
    for (int k = 0; k < 8; k++) {
        int64_t sum = 0;
        for (int n = 0; n < 8; n++)
            sum += (int64_t)v[n] * COS_TABLE[n][k];
        int64_t scale = (k == 0) ? 512 : 724;
        out[k] = (int32_t)((sum * scale + (1LL << 19)) >> 20);
    }
    for (int k = 0; k < 8; k++) v[k] = out[k];
}

static void idct1d(int32_t *v) {
    int32_t scaled[8];
    for (int k = 0; k < 8; k++) {
        int64_t scale = (k == 0) ? 512 : 724;
        scaled[k] = (int32_t)(((int64_t)v[k] * scale + (1 << 9)) >> 10);
    }
    int32_t out[8];
    for (int n = 0; n < 8; n++) {
        int64_t sum = 0;
        for (int k = 0; k < 8; k++)
            sum += (int64_t)scaled[k] * COS_TABLE[n][k];
        out[n] = (int32_t)((sum + (1 << 10)) >> 11);
    }
    for (int n = 0; n < 8; n++) v[n] = out[n];
}

void dct8x8(int16_t block[64]) {
    int32_t tmp[64];
    int32_t row[8];

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++)
            row[c] = (int32_t)block[r * 8 + c];
        dct1d(row);
        for (int c = 0; c < 8; c++)
            tmp[r * 8 + c] = row[c];
    }

    for (int c = 0; c < 8; c++) {
        for (int r = 0; r < 8; r++)
            row[r] = tmp[r * 8 + c];
        dct1d(row);
        for (int r = 0; r < 8; r++)
            tmp[r * 8 + c] = row[r];
    }

    for (int i = 0; i < 64; i++) block[i] = (int16_t)tmp[i];
}

void idct8x8(int16_t block[64]) {
    int32_t tmp[64];
    int32_t row[8];

    for (int c = 0; c < 8; c++) {
        for (int r = 0; r < 8; r++)
            row[r] = (int32_t)block[r * 8 + c];
        idct1d(row);
        for (int r = 0; r < 8; r++)
            tmp[r * 8 + c] = row[r];
    }

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++)
            row[c] = tmp[r * 8 + c];
        idct1d(row);
        for (int c = 0; c < 8; c++)
            tmp[r * 8 + c] = row[c];
    }

    for (int i = 0; i < 64; i++) block[i] = (int16_t)tmp[i];
}

const uint8_t ZIGZAG[64] = {
     0,  1,  8, 16,  9,  2,  3, 10,
    17, 24, 32, 25, 18, 11,  4,  5,
    12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13,  6,  7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36,
    29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46,
    53, 60, 61, 54, 47, 55, 62, 63,
};

const uint8_t IZIGZAG[64] = {
     0,  1,  5,  6, 14, 15, 27, 28,
     2,  4,  7, 13, 16, 26, 29, 42,
     3,  8, 12, 17, 25, 30, 41, 43,
     9, 11, 18, 24, 31, 40, 44, 53,
    10, 19, 23, 32, 39, 45, 52, 54,
    20, 22, 33, 38, 46, 51, 55, 60,
    21, 34, 37, 47, 50, 56, 59, 61,
    35, 36, 48, 49, 57, 58, 62, 63,
};
