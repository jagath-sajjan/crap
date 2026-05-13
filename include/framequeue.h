#ifndef FRAMEQUEUE_H
#define FRAMEQUEUE_H

#include "frame.h"
#include <stdint.h>
#include <stddef.h>

/*
 * Lock-free SPSC ring buffer queue of CrapFrame pointers
 *
 * Single producer / single consumer only.
 * No mutex required in that usage pattern.
 *
 * For MPSC/MPMC usage, add a spinlock or atomics later.
 *
 * Capacity must be a power of two, enforced by fq_init().
 * Stores pointers, not copies. Frames live in the pool.
 */

#define FRAMEQUEUE_MAX_CAPACITY 256 // hard cap, must be power of two

typedef struct {
    CrapFrame *slots[FRAMEQUEUE_MAX_CAPACITY];

    uint32_t   capacity; // actual capacity (<= MAX, power of 2)
    uint32_t   mask;     // capacity - 1, for fast modulo

    /*
     * head: next slot to read  (consumer increments)
     * tail: next slot to write (producer increments)
     *
     * Both grow monotonically and wrap via mask.
     *
     * volatile prevents compiler reordering
     * across producer / consumer threads.
     */
    volatile uint32_t head;
    volatile uint32_t tail;
} FrameQueue;

/*
 * Initialise queue with given capacity.
 *
 * Capacity is rounded up to the next power of two.
 *
 * Returns:
 * - 0 on success
 * - -1 if capacity exceeds FRAMEQUEUE_MAX_CAPACITY
 */
int fq_init(FrameQueue *q, uint32_t capacity);

/*
 * Reset queue state.
 *
 * Does not free or destroy queued frames.
 */
void fq_reset(FrameQueue *q);

/*
 * Push frame pointer into queue.
 *
 * Returns:
 * - 0 on success
 * - -1 if queue is full
 *
 * Call from producer thread only.
 */
int fq_push(FrameQueue *q, CrapFrame *frame);

/*
 * Pop frame pointer from queue.
 *
 * Returns NULL if queue is empty.
 *
 * Call from consumer thread only.
 */
CrapFrame *fq_pop(FrameQueue *q);

/*
 * Peek at next frame without consuming it.
 *
 * Returns NULL if queue is empty.
 */
CrapFrame *fq_peek(const FrameQueue *q);

static inline int fq_empty(const FrameQueue *q) {
    return q->head == q->tail;
}

static inline uint32_t fq_size(const FrameQueue *q) {
    return q->tail - q->head;
}

static inline int fq_full(const FrameQueue *q) {
    return fq_size(q) == q->capacity;
}

#endif //FRAMEQUEUE_H
