#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    uint8_t  *buf;
    size_t    cap;
    size_t    byte_pos;
    uint8_t   bit_pos;
} BSWriter;

static inline void bsw_init(BSWriter *w, uint8_t *buf, size_t cap) {
    w->buf      = buf;
    w->cap      = cap;
    w->byte_pos = 0;
    w->bit_pos  = 7;
    memset(buf, 0, cap);
}

static inline size_t bsw_bits_written(const BSWriter *w) {
    return w->byte_pos * 8 + (7 - w->bit_pos);
}

static inline size_t bsw_bytes_written(const BSWriter *w) {
    size_t bits = bsw_bits_written(w);
    return (bits + 7) / 8;
}

static inline int bsw_write_bit(BSWriter *w, int bit) {
    if (w->byte_pos >= w->cap) return -1;
    if (bit) w->buf[w->byte_pos] |= (uint8_t)(1u << w->bit_pos);
    if (w->bit_pos == 0) {
        w->byte_pos++;
        w->bit_pos = 7;
    } else {
        w->bit_pos--;
    }
    return 0;
}

static inline int bsw_write_bits(BSWriter *w, uint32_t val, int n) {
    for (int i = n - 1; i >= 0; i--) {
        if (bsw_write_bit(w, (val >> i) & 1) != 0) return -1;
    }
    return 0;
}

static inline int bsw_write_byte(BSWriter *w, uint8_t val) {
    if (w->bit_pos == 7) {
        if (w->byte_pos >= w->cap) return -1;
        w->buf[w->byte_pos++] = val;
        return 0;
    }
    return bsw_write_bits(w, val, 8);
}

static inline int bsw_write_u16(BSWriter *w, uint16_t val) {
    if (bsw_write_byte(w, (uint8_t)(val >> 8))    != 0) return -1;
    if (bsw_write_byte(w, (uint8_t)(val & 0xFF))  != 0) return -1;
    return 0;
}

static inline int bsw_write_u32(BSWriter *w, uint32_t val) {
    if (bsw_write_u16(w, (uint16_t)(val >> 16))    != 0) return -1;
    if (bsw_write_u16(w, (uint16_t)(val & 0xFFFF)) != 0) return -1;
    return 0;
}

static inline void bsw_align(BSWriter *w) {
    if (w->bit_pos != 7) {
        w->byte_pos++;
        w->bit_pos = 7;
    }
}

typedef struct {
    const uint8_t *buf;
    size_t         len;
    size_t         byte_pos;
    uint8_t        bit_pos;
} BSReader;

static inline void bsr_init(BSReader *r, const uint8_t *buf, size_t len) {
    r->buf      = buf;
    r->len      = len;
    r->byte_pos = 0;
    r->bit_pos  = 7;
}

static inline size_t bsr_bits_left(const BSReader *r) {
    if (r->byte_pos >= r->len) return 0;
    return (r->len - r->byte_pos) * 8 - (7 - r->bit_pos);
}

static inline int bsr_read_bit(BSReader *r) {
    if (r->byte_pos >= r->len) return -1;
    int bit = (r->buf[r->byte_pos] >> r->bit_pos) & 1;
    if (r->bit_pos == 0) {
        r->byte_pos++;
        r->bit_pos = 7;
    } else {
        r->bit_pos--;
    }
    return bit;
}

static inline int bsr_read_bits(BSReader *r, int n, uint32_t *out) {
    uint32_t val = 0;
    for (int i = n - 1; i >= 0; i--) {
        int b = bsr_read_bit(r);
        if (b < 0) return -1;
        val |= (uint32_t)b << i;
    }
    *out = val;
    return 0;
}

static inline int bsr_read_byte(BSReader *r, uint8_t *out) {
    if (r->bit_pos == 7) {
        if (r->byte_pos >= r->len) return -1;
        *out = r->buf[r->byte_pos++];
        return 0;
    }
    uint32_t val;
    if (bsr_read_bits(r, 8, &val) != 0) return -1;
    *out = (uint8_t)val;
    return 0;
}

static inline int bsr_read_u16(BSReader *r, uint16_t *out) {
    uint8_t hi, lo;
    if (bsr_read_byte(r, &hi) != 0) return -1;
    if (bsr_read_byte(r, &lo) != 0) return -1;
    *out = (uint16_t)((hi << 8) | lo);
    return 0;
}

static inline int bsr_read_u32(BSReader *r, uint32_t *out) {
    uint16_t hi, lo;
    if (bsr_read_u16(r, &hi) != 0) return -1;
    if (bsr_read_u16(r, &lo) != 0) return -1;
    *out = ((uint32_t)hi << 16) | lo;
    return 0;
}

static inline void bsr_align(BSReader *r) {
    if (r->bit_pos != 7) {
        r->byte_pos++;
        r->bit_pos = 7;
    }
}

static inline int bsr_skip_bits(BSReader *r, int n) {
    uint32_t dummy;
    return bsr_read_bits(r, n, &dummy);
}

#endif // BITSTREAM_H
