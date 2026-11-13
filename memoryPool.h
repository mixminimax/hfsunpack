#ifndef MEMORYPOOL_F9CE5273_252E_4F94_BEF6_F4ECCBAB1D31_H
#define MEMORYPOOL_F9CE5273_252E_4F94_BEF6_F4ECCBAB1D31_H

#include <stddef.h> // size_t

#ifndef MemPool
typedef void MemPool;
#endif // MemPool

/**
 * @brief Create a new memory arena.
 *
 * @param heapSize Usable size in bytes. Must be larger than 0.
 * @return void* Pool instance if success otherwise NULL
 *
 * @note The pool internally allocates a multiple of the system page size to
 * fill best performance. If it allocates more memory then requested, it is
 * not a bug.
 *
 */
void *memPool_init(size_t heapSize);

/**
 * @brief Destroy the pool and release its backing region.
 *
 * All pointers previously returned by memPool_alloc() become invalid.
 *
 * @param pool Pool instance. NULL is accepted.
 */
void memPool_destroy(MemPool *pool);

/**
 * @brief Allocate `size` bytes from the pool.
 *
 * The pool is a bump (arena) allocator: every allocation is carved out of one
 * contiguous, page-aligned region. There is no per-allocation bookkeeping, no
 * copying and no fragmentation. Returned pointers are aligned to 64 bytes.
 *
 * Allocations live until memPool_reset() or memPool_destroy() is called.
 *
 * @param pool Pool instance.
 * @param size Number of bytes. Must be within the remaining capacity.
 * @return void* Allocated region, or NULL if the pool is exhausted
 */
void *memPool_alloc(MemPool *pool, size_t size);

/**
 * @brief Discard all live allocations and make the whole capacity available
 * again.
 *
 * After this call, only the pool handle itself remains valid; every pointer
 * previously returned by memPool_alloc() must not be used.
 *
 * @param pool Pool instance. NULL is accepted.
 */
void memPool_reset(MemPool *pool);

/**
 * @brief Bytes allocated since the last memPool_reset() (or init).
 *
 * @param pool Pool instance. NULL reports 0.
 */
size_t memPool_used(const MemPool *pool);

/**
 * @brief Total usable capacity of the pool in bytes.
 *
 * @param pool Pool instance. NULL reports 0.
 */
size_t memPool_capacity(const MemPool *pool);

#endif // MEMORYPOOL_F9CE5273_252E_4F94_BEF6_F4ECCBAB1D31_H
