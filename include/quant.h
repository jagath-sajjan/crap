#ifndef QUANT_H
#define QUANT_H

#include "dct.h"
#include <stdint.h>

/*
 * luma & chroma quality scaled at runtime
 * values are divisors [larger = more lossy]
*/

extern const uint8_t QUANT_LUMA_BASE[64];
extern const uint8_t QUANT_CHROMA_BASE[64];

/*
 * scaled quantization table, derived from base + quality level
 * quality: 1 (worst) .. 100 (lossless-ish)
 * out: 64 uint16_t divisors in natural order
 */
void quant_build_table(const uint8_t base[64], int quality, uint16_t out[64]);

/*
 * quantize an 8x8 DCT coefficient block in place
 * applies zigzag reordering output is in zigzag order
 */
void quant_encode(int16_t block[64], const uint16_t qtable[64]);


/*
 * dequantize an 8x8 block in place
 * multiplies each zigzag ordered coefficient by the table entry
 */
void quant_decode(int16_t block[64], const uint16_t qtable[64]);

#endif /* QUANT_H */
