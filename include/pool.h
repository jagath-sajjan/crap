#ifndef POOL_H
#define POOL_H

#include <stdint.h>
#include <stddef.h>
#include "crap.h"

/*
 * Fixed size slab memory pool
 *
 * All blocks are the same size (block_size). The pool allocates
 * one large contiguous slab up front. Free blocks are tracked
 * via an intrusive singly linked freelist embedded in the blocks
 * themselves. No metadata overhead per live block.
 *
 * Properties:
 * - O(1) alloc & free
 * - zero fragmentation (fixed block size)
 * - cache friendly (contiguous slab)
 * - no per block malloc overhead
 * - not thread safe (external locking if needed)
 */

typedef struct PoolBlock {
    struct PoolBlock *next; // freelist link, valid only when free
} PoolBlock;

typedef struct {
    uint8_t   *slab;        // base of allocated memory
    PoolBlock *free_head;   // head of freelist

    size_t     block_size;  // bytes per block [>= sizeof(void *)]
    size_t     block_count; // total blocks in pool
    size_t     free_count;  // currently available blocks
} MemPool;

/*
 * Initialise a pool.
 * Allocates block_count * block_size bytes.
 *
 * block_size is rounded up to pointer alignment internally.
 *
 * Returns:
 * - 0 on success
 * - CRAP_ERR_OOM on allocation failure
 */
int pool_init(
    MemPool *pool,
    size_t   block_size,
    size_t   block_count
);

/*
 * Destroy pool and free the underlying slab.
 *
 * All blocks must have been returned before calling this.
 * No use-after-free detection is performed.
 */
void pool_destroy(MemPool *pool);

/*
 * Acquire one block.
 *
 * Returns NULL if the pool is exhausted.
 * Returned memory is uninitialised.
 */
void *pool_acquire(MemPool *pool);

/*
 * Return a block to the pool.
 *
 * ptr must have come from pool_acquire()
 * on the same pool instance.
 */
void pool_release(MemPool *pool, void *ptr);

static inline size_t pool_free_count(const MemPool *pool) {
    return pool->free_count;
}

static inline size_t pool_used_count(const MemPool *pool) {
    return pool->block_count - pool->free_count;
}

#endif // POOL_H
