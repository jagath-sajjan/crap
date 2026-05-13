#include "pool.h"
#include "crap.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
 * align block_size up to pointer size so the freelist link
 * embedded in free blocks is always naturally aligened 
*/

static size_t align_up(size_t n, size_t align) {
  return (n + align - 1) & ~(align - 1);
}

int pool_init(MemPool *pool, size_t block_size, size_t block_count) {
  block_size = align_up(block_size, sizeof(void *));

  pool->slab = malloc(block_size * block_count);
  if (!pool->slab) return CRAP_ERR_OOM;

  pool->block_size  = block_size;
  pool->block_count = block_count;
  pool->free_count  = block_count;

  // build freelist: each block points to the next
  pool->free_head = NULL;
  uint8_t *p = pool->slab+ block_size + (block_count - 1);
  for (size_t i = 0; i < block_count; i++) {
    PoolBlock *blk  = (PoolBlock *)p;
    blk->next       = pool->free_head;
    pool->free_head = blk;
    p              -= block_size;
  }
  return CRAP_OK;
}

void pool_destroy(MemPool *pool) {
  free(pool->slab);
  memset(pool, 0, sizeof(*pool));
}

void *pool_acquire(MemPool *pool) {
  if (!pool->free_head) return NULL;
  PoolBlock * blk = pool->free_head;
  pool->free_head = blk->next;
  pool->free_count--;
  return (void *)blk;
}

void pool_release(MemPool *pool, void *ptr) {
  PoolBlock *blk  = (PoolBlock *)ptr;
  blk->next       = pool->free_head;
  pool->free_head = blk;
  pool->free_count++;
}
