
#include "entropy.h"

#include <stdint.h>

#include <string.h>



static inline int coeff_size(int val) {

    if (val < 0) val = -val;

    int n = 0;

    while (val > 0) { n++; val >>= 1; }

    return n;

}



static inline int write_coeff(BSWriter *w, int val, int size) {

    if (size == 0) return 0;

    uint32_t bits = (val > 0)

        ? (uint32_t)val

        : (uint32_t)(val + (1 << size) - 1);

    return bsw_write_bits(w, bits, size);

}



static inline int read_coeff(BSReader *r, int size, int16_t *out) {

    if (size == 0) { *out = 0; return 0; }

    uint32_t bits;

    if (bsr_read_bits(r, size, &bits) != 0) return -1;

    int thr = 1 << (size - 1);

    *out = (bits >= (uint32_t)thr)

        ? (int16_t)bits

        : (int16_t)((int)bits - (1 << size) + 1);

    return 0;

}



int entropy_encode_block(BSWriter *w, const int16_t block[64]) {

    int dc_size = coeff_size(block[0]);

    if (bsw_write_bits(w, (uint32_t)dc_size, 4) != 0) return -1;

    if (write_coeff(w, block[0], dc_size)        != 0) return -1;



    int run = 0;

    for (int i = 1; i < 64; i++) {

        int val = block[i];

        if (val == 0) {

            run++;

            if (run == 16) {

                if (bsw_write_bits(w, 15, 4) != 0) return -1;

                if (bsw_write_bits(w,  0, 4) != 0) return -1;

                run = 0;

            }

        } else {

            int size = coeff_size(val);

            if (bsw_write_bits(w, (uint32_t)run,  4) != 0) return -1;

            if (bsw_write_bits(w, (uint32_t)size, 4) != 0) return -1;

            if (write_coeff(w, val, size)             != 0) return -1;

            run = 0;

        }

    }

    /* always write EOB — decoder must always consume it */

    if (bsw_write_bits(w, 0, 4) != 0) return -1;

    if (bsw_write_bits(w, 0, 4) != 0) return -1;

    bsw_align(w);

    return 0;

}



int entropy_decode_block(BSReader *r, int16_t block[64]) {

    memset(block, 0, 64 * sizeof(int16_t));



    uint32_t dc_size;

    if (bsr_read_bits(r, 4, &dc_size) != 0) return -1;

    if (read_coeff(r, (int)dc_size, &block[0]) != 0) return -1;



    int i = 1;

    int got_eob = 0;

    while (i < 64) {

        uint32_t run_bits, size_bits;

        if (bsr_read_bits(r, 4, &run_bits)  != 0) return -1;

        if (bsr_read_bits(r, 4, &size_bits) != 0) return -1;



        int run  = (int)run_bits;

        int size = (int)size_bits;



        if (run == 0 && size == 0) { got_eob = 1; break; }

        if (run == 15 && size == 0) { i += 16; continue; }



        i += run;

        if (i >= 64) return -1;

        if (read_coeff(r, size, &block[i]) != 0) return -1;

        i++;

    }

    /* if loop exited naturally (i==64), still consume the EOB */

    if (!got_eob) {

        uint32_t rb, sb;

        if (bsr_read_bits(r, 4, &rb) != 0) return -1;

        if (bsr_read_bits(r, 4, &sb) != 0) return -1;

        /* must be EOB */

        if (rb != 0 || sb != 0) return -1;

    }

    bsr_align(r);

    return 0;

}

