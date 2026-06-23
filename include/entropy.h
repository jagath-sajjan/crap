#ifndef ENTROPY_H
#define ENTROPY_H

#include "bitstream.h"
#include <stdint.h>
#include <stddef.h>

/*
 * encode one 8x8 block into BSWriter
 * block[0] = DC, block[1..63] = AC in zigzag order
 * returns 0 on success, -1 on buffer overflow
 */
int entropy_encode_block(BSWriter *w, const int16_t block[64]);

/*
 * decode one 8x8 block from BSReader into block[64] (zigzag order)
 * returns 0 on success, -1 on stream error
 */
int entropy_decode_block(BSReader *r, int16_t block[64]);

#endif /* ENTROPY_H */
