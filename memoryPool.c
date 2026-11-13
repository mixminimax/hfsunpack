#include "memoryPool.h"

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else // POSIX
#  include <sys/mman.h>
#endif // _WIN32

// Every allocation is aligned to this many bytes. 64 keeps results cache-line
// aligned and matches the 64-byte ChaCha20 block the reader (packetReader.c)
// works in.
#define MEMPOOL_ALIGN 64u

// System page size used to round the backing region up.
#define MEMPOOL_PAGE 4096u

// The descriptor is placed at the very start of the backing region, so a
// whole pool is exactly one page-aligned allocation. memPool_init() returns
// this struct as an opaque handle; the usable arena starts at arenaBase.
struct PoolContext
{
  size_t capacity;  // Usable bytes (arenaBase .. end of region)
  size_t used;      // Bump offset, always in [arenaBase, capacity]
  size_t arenaBase; // First usable offset (end of this descriptor, aligned)
};

static size_t memPool_alignUp(size_t value, size_t alignment)
{
  return (value + (alignment - 1)) & ~(alignment - 1);
}

static void *memPool_reserve(size_t size)
{
#ifdef _WIN32
  void *base =
      VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  return (base != NULL) ? base : NULL;
#else // POSIX
  void *base = mmap(NULL,
                    size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS,
                    -1,
                    0);
  return (base != MAP_FAILED) ? base : NULL;
#endif // _WIN32
}

static void memPool_release(void *base, size_t size)
{
#ifdef _WIN32
  (void)VirtualFree(base, 0, MEM_RELEASE);
#else // POSIX
  (void)munmap(base, size);
#endif // _WIN32
}

void *memPool_init(size_t heapSize)
{
  if ( !heapSize )
  {
    return NULL;
  }

  size_t arenaBase = memPool_alignUp(sizeof(struct PoolContext), MEMPOOL_ALIGN);
  size_t need      = arenaBase + heapSize;
  size_t req       = memPool_alignUp(need, MEMPOOL_PAGE);

  void *base = memPool_reserve(req);
  if ( !base )
  {
    return NULL;
  }

  struct PoolContext *pool = (struct PoolContext *)base;
  pool->arenaBase          = arenaBase;
  pool->capacity           = req - arenaBase;
  pool->used               = arenaBase;
  return pool;
}

void *memPool_alloc(MemPool *pool, size_t size)
{
  if ( !pool || !size )
  {
    return NULL;
  }

  struct PoolContext *ctx = (struct PoolContext *)pool;
  size_t              next = memPool_alignUp(ctx->used, MEMPOOL_ALIGN);

  // End of the usable arena (offset of the last usable byte + 1).
  size_t end = ctx->arenaBase + ctx->capacity;
  if ( (next > end) || ((end - next) < size) )
  {
    return NULL; // Pool exhausted
  }

  void *result = (void *)((char *)pool + next);
  ctx->used    = next + size;
  return result;
}

void memPool_reset(MemPool *pool)
{
  if ( !pool )
  {
    return;
  }
  ((struct PoolContext *)pool)->used = ((struct PoolContext *)pool)->arenaBase;
}

size_t memPool_used(const MemPool *pool)
{
  if ( !pool )
  {
    return 0;
  }
  const struct PoolContext *ctx = (const struct PoolContext *)pool;
  return ctx->used - ctx->arenaBase;
}

size_t memPool_capacity(const MemPool *pool)
{
  if ( !pool )
  {
    return 0;
  }
  return ((const struct PoolContext *)pool)->capacity;
}

void memPool_destroy(MemPool *pool)
{
  if ( !pool )
  {
    return;
  }

  const struct PoolContext *ctx = (const struct PoolContext *)pool;
  memPool_release((void *)pool, ctx->capacity + ctx->arenaBase);
}
