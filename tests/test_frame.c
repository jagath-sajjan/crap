#include "frame.h"
#include "pool.h"
#include "framequeue.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static void test_pool(void) {
    MemPool pool;
    size_t block_sz = frame_buffer_size(PIX_FMT_YUV420P, 16, 16);
    assert(pool_init(&pool, block_sz, 4) == CRAP_OK);
    assert(pool_free_count(&pool) == 4);

    void *a = pool_acquire(&pool); assert(a);
    void *b = pool_acquire(&pool); assert(b);
    assert(pool_free_count(&pool) == 2);

    pool_release(&pool, a);
    assert(pool_free_count(&pool) == 3);

    void *c = pool_acquire(&pool); assert(c);
    void *d = pool_acquire(&pool); assert(d);
    void *e = pool_acquire(&pool); assert(e);
    void *f = pool_acquire(&pool); assert(!f); /* exhausted */

    pool_release(&pool, b);
    pool_release(&pool, c);
    pool_release(&pool, d);
    pool_release(&pool, e);
    pool_destroy(&pool);
    printf("[PASS] pool\n");
}

static void test_frame_attach(void) {
    uint8_t buf[frame_buffer_size(PIX_FMT_YUV420P, 64, 64)];
    CrapFrame f;
    frame_init(&f);
    frame_attach_buffer(&f, PIX_FMT_YUV420P, 64, 64, buf);

    assert(f.plane[0].width  == 64 && f.plane[0].height == 64);
    assert(f.plane[1].width  == 32 && f.plane[1].height == 32);
    assert(f.plane[2].width  == 32 && f.plane[2].height == 32);
    assert(f.plane[1].data == buf + 64*64);
    assert(f.plane[2].data == buf + 64*64 + 32*32);
    assert(f.ref[0] == -1 && f.ref[1] == -1);
    printf("[PASS] frame_attach YUV420P\n");
}

static void test_framequeue(void) {
    FrameQueue q;
    assert(fq_init(&q, 4) == 0);
    assert(q.capacity == 4);
    assert(fq_empty(&q));

    CrapFrame frames[4];
    for (int i = 0; i < 4; i++) {
        frame_init(&frames[i]);
        frames[i].frame_number = (uint64_t)i;
        assert(fq_push(&q, &frames[i]) == 0);
    }
    assert(fq_full(&q));
    assert(fq_push(&q, &frames[0]) == -1); /* full */

    assert(fq_peek(&q)->frame_number == 0);

    for (int i = 0; i < 4; i++) {
        CrapFrame *out = fq_pop(&q);
        assert(out && out->frame_number == (uint64_t)i);
    }
    assert(fq_empty(&q));
    printf("[PASS] framequeue\n");
}

int main(void) {
    test_pool();
    test_frame_attach();
    test_framequeue();
    printf("Stage 2 tests passed.\n");
    return 0;
}
