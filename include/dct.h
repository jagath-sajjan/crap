#ifndef DCT_H
#define DCT_H

#include <stdint.h>

void dct8x8(int16_t block[64]);
void idct8x8(int16_t block[64]);

extern const uint8_t ZIGZAG[64];
extern const uint8_t IZIGZAG[64];

#endif /* DCT_H */
