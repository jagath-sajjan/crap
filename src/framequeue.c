#include "framequeue.h"
#include <string.h>

/*
 * Round up to next power of two.
 *
 * Used so modulo operations can become:
 *   index & (capacity - 1)
 */
static uint32_t next_pow2(uint32_t n) {
    if (n == 0) {
        return 1;
    }

    n--;

    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;

    return n + 1;
}

int fq_init(FrameQueue *q, uint32_t capacity) {
    capacity = next_pow2(capacity);

    if (capacity > FRAMEQUEUE_MAX_CAPACITY) {
        return -1;
    }

    memset(q, 0, sizeof(*q));

    q->capacity = capacity;
    q->mask     = capacity - 1;

    return 0;
}

void fq_reset(FrameQueue *q) {
    q->head = 0;
    q->tail = 0;
}

int fq_push(FrameQueue *q, CrapFrame *frame) {
    if (fq_full(q)) {
        return -1;
    }

    q->slots[q->tail & q->mask] = frame;

    /*
     * Compiler barrier prevents reordering
     * of slot write and tail increment.
     */
    __asm__ volatile("" ::: "memory");

    q->tail++;

    return 0;
}

CrapFrame *fq_pop(FrameQueue *q) {
    if (fq_empty(q)) {
        return NULL;
    }

    CrapFrame *f = q->slots[q->head & q->mask];

    /*
     * Compiler barrier prevents reordering
     * of slot read and head increment.
     */
    __asm__ volatile("" ::: "memory");

    q->head++;

    return f;
}

CrapFrame *fq_peek(const FrameQueue *q) {
    if (fq_empty(q)) {
        return NULL;
    }

    return q->slots[q->head & q->mask];
}
