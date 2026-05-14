#include "bitstream.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_single_bits(void) {
    uint8_t buf[4];
    BSWriter w;
    bsw_init(&w, buf, sizeof(buf));
    bsw_write_bit(&w, 1); bsw_write_bit(&w, 0); bsw_write_bit(&w, 1);
    bsw_write_bit(&w, 1); bsw_write_bit(&w, 0); bsw_write_bit(&w, 1);
    bsw_write_bit(&w, 0); bsw_write_bit(&w, 0);
    assert(bsw_bytes_written(&w) == 1);
    assert(buf[0] == 0xB4);
    BSReader r;
    bsr_init(&r, buf, 1);
    assert(bsr_read_bit(&r) == 1); assert(bsr_read_bit(&r) == 0);
    assert(bsr_read_bit(&r) == 1); assert(bsr_read_bit(&r) == 1);
    assert(bsr_read_bit(&r) == 0); assert(bsr_read_bit(&r) == 1);
    assert(bsr_read_bit(&r) == 0); assert(bsr_read_bit(&r) == 0);
    assert(bsr_read_bit(&r) == -1);
    printf("[PASS] single bits\n");
}

static void test_multi_bits(void) {
    uint8_t buf[8];
    BSWriter w;
    bsw_init(&w, buf, sizeof(buf));
    bsw_write_bits(&w, 0x5,  3);
    bsw_write_bits(&w, 0x1A, 5);
    bsw_write_bits(&w, 0xFF, 8);
    assert(buf[0] == 0xBA);
    assert(buf[1] == 0xFF);
    BSReader r;
    bsr_init(&r, buf, sizeof(buf));
    uint32_t val;
    bsr_read_bits(&r, 3, &val); assert(val == 0x5);
    bsr_read_bits(&r, 5, &val); assert(val == 0x1A);
    bsr_read_bits(&r, 8, &val); assert(val == 0xFF);
    printf("[PASS] multi bits\n");
}

static void test_byte_aligned(void) {
    uint8_t buf[8];
    BSWriter w;
    bsw_init(&w, buf, sizeof(buf));
    bsw_write_byte(&w, 0xDE); bsw_write_byte(&w, 0xAD);
    bsw_write_byte(&w, 0xBE); bsw_write_byte(&w, 0xEF);
    assert(buf[0]==0xDE && buf[1]==0xAD && buf[2]==0xBE && buf[3]==0xEF);
    assert(bsw_bytes_written(&w) == 4);
    BSReader r;
    bsr_init(&r, buf, 4);
    uint8_t b;
    bsr_read_byte(&r, &b); assert(b == 0xDE);
    bsr_read_byte(&r, &b); assert(b == 0xAD);
    bsr_read_byte(&r, &b); assert(b == 0xBE);
    bsr_read_byte(&r, &b); assert(b == 0xEF);
    assert(bsr_read_byte(&r, &b) == -1);
    printf("[PASS] byte aligned\n");
}

static void test_u32_roundtrip(void) {
    uint8_t buf[16];
    BSWriter w;
    bsw_init(&w, buf, sizeof(buf));
    bsw_write_u32(&w, 0xDEADBEEF);
    bsw_write_u32(&w, 0x00C0FFEE);
    BSReader r;
    bsr_init(&r, buf, sizeof(buf));
    uint32_t val;
    bsr_read_u32(&r, &val); assert(val == 0xDEADBEEF);
    bsr_read_u32(&r, &val); assert(val == 0x00C0FFEE);
    printf("[PASS] u32 roundtrip\n");
}

static void test_align(void) {
    uint8_t buf[4];
    BSWriter w;
    bsw_init(&w, buf, sizeof(buf));
    bsw_write_bits(&w, 0x5, 3);
    bsw_align(&w);
    bsw_write_byte(&w, 0xAB);
    assert(buf[0] == 0xA0);
    assert(buf[1] == 0xAB);
    printf("[PASS] align\n");
}

static void test_overflow(void) {
    uint8_t buf[1];
    BSWriter w;
    bsw_init(&w, buf, sizeof(buf));
    assert(bsw_write_byte(&w, 0xFF) == 0);
    assert(bsw_write_byte(&w, 0xFF) == -1);
    printf("[PASS] overflow guard\n");
}

int main(void) {
    test_single_bits();
    test_multi_bits();
    test_byte_aligned();
    test_u32_roundtrip();
    test_align();
    test_overflow();
    printf("Bitstream tests passed.\n");
    return 0;
}
