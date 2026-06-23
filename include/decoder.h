#ifndef DECODER_H
#define DECODER_H

#include "container.h"
#include "frame.h"
#include "pool.h"
#include "quant.h"
#include <stdint.h>

#define DECODER_POOL_FRAMES 8

typedef struct {
    CrapContext  ctx;
    uint16_t     qtable_luma[64];
    uint16_t     qtable_chroma[64];
    uint32_t     width;
    uint32_t     height;
    MemPool      frame_pool;
} CrapDecoder;

int        decoder_open         (CrapDecoder *dec, const char *path, int quality);
CrapFrame *decoder_next_frame   (CrapDecoder *dec);
void       decoder_release_frame(CrapDecoder *dec, CrapFrame *f);
void       decoder_close        (CrapDecoder *dec);

#endif /* DECODER_H */
