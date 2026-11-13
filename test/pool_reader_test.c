// Tests for memoryPool.c and the pool-backed packetReader.c.
//
// Two parts:
//  1. Pool unit tests (alignment, capacity, exhaustion, reset, destroy).
//  2. Differential test: the reference implementation (refPacket_*, the old
//     realloc-based reader captured before the rewrite) and the new
//     pool-backed reader (encPacket_*) must produce byte-identical output,
//     identical per-call return values and identical final positions for the
//     same read sequences. This matters because the reader emulates a quirky
//     ChaCha20 counter protocol (counter sometimes resets to 0) that must be
//     preserved exactly.

#include "memoryPool.h"
#include "packetReader.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- Reference (old, realloc-based) reader, renamed copy in
// ref_packetReader.c
void *refPacket_init(void   *src,
                     uint64_t counter,
                     uint8_t  nonce[8],
                     uint8_t  key[32]);
void refPacket_delete(void **reader);
int  refPacket_read(void *reader, void *dest, uint32_t size);
int  refPacket_position(void *reader);

static int  g_checks   = 0;
static int  g_failures = 0;

#define CHECK(cond, ...)                                            \
  do                                                                \
  {                                                                 \
    ++g_checks;                                                     \
    if ( !(cond) )                                                  \
    {                                                               \
      ++g_failures;                                                 \
      printf("FAIL %s:%d: ", __FILE__, __LINE__);                   \
      printf(__VA_ARGS__);                                          \
      printf("\n");                                                 \
    }                                                               \
  } while ( 0 )

// Deterministic PRNG (xorshift64) so runs are reproducible.
static uint64_t g_rng = 0x9E3779B97F4A7C15ULL;

static uint64_t nextRand(void)
{
  g_rng ^= g_rng << 13;
  g_rng ^= g_rng >> 7;
  g_rng ^= g_rng << 17;
  return g_rng;
}

// ---------------------------------------------------------------------------
// 1. Pool unit tests
// ---------------------------------------------------------------------------

static void testPoolBasics(void)
{
  printf("== pool: basics ==\n");
  CHECK( memPool_init(0) == NULL, "init(0) must fail" );

  MemPool *pool = memPool_init(4096);
  CHECK( pool != NULL, "init(4096) failed" );
  if ( !pool )
  {
    return;
  }

  size_t cap = memPool_capacity(pool);
  CHECK( cap >= 4096, "capacity %zu < requested 4096", cap );

  // Allocation 1: alignment + content usable.
  void *a1 = memPool_alloc(pool, 100);
  CHECK( a1 != NULL, "alloc(100) failed" );
  if ( a1 )
  {
    CHECK( ((uintptr_t)a1 % 64) == 0, "a1 not 64-aligned: %p", a1 );
    memset(a1, 0xA5, 100);
    CHECK( ((uint8_t *)a1)[99] == 0xA5, "a1 not writable" );
  }

  // Allocation 2: distinct, aligned, non-overlapping.
  void *a2 = memPool_alloc(pool, 200);
  CHECK( a2 != NULL, "alloc(200) failed" );
  if ( a1 && a2 )
  {
    CHECK( ((uintptr_t)a2 % 64) == 0, "a2 not 64-aligned: %p", a2 );
    CHECK( a2 != a1, "a2 == a1" );
    CHECK( ( (uint8_t *)a2 > (uint8_t *)a1 + 100 ), "a2 overlaps a1" );
  }

  CHECK( memPool_used(pool) >= 300, "used %zu < 300", memPool_used(pool) );

  // Exhaustion: ask for more than what remains -> NULL, pool stays usable.
  void *a3 = memPool_alloc(pool, cap);
  CHECK( a3 == NULL, "alloc(capacity) after others must fail" );
  void *a4 = memPool_alloc(pool, 1);
  CHECK( a4 != NULL, "alloc(1) after failed large alloc must work" );

  // Reset: whole capacity is available again.
  memPool_reset(pool);
  CHECK( memPool_used(pool) == 0, "used != 0 after reset" );
  void *a5 = memPool_alloc(pool, cap);
  CHECK( a5 != NULL, "alloc(capacity) after reset must work" );
  if ( a5 )
  {
    CHECK( ((uintptr_t)a5 % 64) == 0, "a5 not aligned" );
    memset(a5, 0x5A, cap); // touch every byte
    CHECK( ((uint8_t *)a5)[cap - 1] == 0x5A, "arena not fully usable" );
  }

  memPool_destroy(pool);
  memPool_destroy(NULL); // must not crash
  printf("   (%d checks)\n", g_checks);
}

// ---------------------------------------------------------------------------
// 2. Differential reader test (old realloc reader vs new pool reader)
// ---------------------------------------------------------------------------

// Pseudo-random "ciphertext" the readers decrypt. Must exceed the largest
// total a test scenario may pull from it (random-mixed: ~6.2 MiB + slack).
#define SRC_SIZE (size_t)0x4000000 // 64 MiB

static uint8_t  g_key[32];
static uint8_t  g_nonce[8];

// Static src buffer (filled with PRNG data, acts as arbitrary ciphertext).
static uint8_t g_src[SRC_SIZE];

static void runSequence(void      *reader,
                        int        isRef,
                        const uint32_t *reqs,
                        int       nreq,
                        uint8_t   *cap,
                        uint32_t  *capRead)
{
  size_t off = 0;
  for ( int i = 0; i < nreq; ++i )
  {
    int got = isRef ? refPacket_read(reader, cap + off, reqs[i])
                    : encPacket_read(reader, cap + off, reqs[i]);
    capRead[i] = (uint32_t)got;
    off += (size_t)got;
  }
}

// Runs one scenario on BOTH readers and compares everything observable.
static int runScenario(const char     *name,
                       const uint32_t *reqs,
                       int            nreq,
                       uint64_t       startCounter)
{
  size_t total = 0;
  for ( int i = 0; i < nreq; ++i )
  {
    total += reqs[i];
  }

  uint8_t *capNew = (uint8_t *)malloc(total ? total : 1);
  uint8_t *capRef = (uint8_t *)malloc(total ? total : 1);
  uint32_t *readNew = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)nreq);
  uint32_t *readRef = (uint32_t *)malloc(sizeof(uint32_t) * (size_t)nreq);
  if ( !capNew || !capRef || !readNew || !readRef )
  {
    free(capNew);
    free(capRef);
    free(readNew);
    free(readRef);
    return -1;
  }

  void *newReader =
      encPacket_init(g_src, startCounter, g_nonce, g_key);
  void *refReader = refPacket_init(g_src, startCounter, g_nonce, g_key);
  if ( !newReader || !refReader )
  {
    printf("FAIL %s: init failed\n", name);
    free(capNew);
    free(capRef);
    free(readNew);
    free(readRef);
    return -1;
  }

  runSequence(newReader, 0, reqs, nreq, capNew, readNew);
  runSequence(refReader, 1, reqs, nreq, capRef, readRef);

  int ok = 1;
  for ( int i = 0; i < nreq && ok; ++i )
  {
    if ( readNew[i] != readRef[i] )
    {
      printf("FAIL %s: req[%d]=%u new=%u ref=%u\n",
             name, i, reqs[i], readNew[i], readRef[i]);
      ok = 0;
    }
  }
  size_t capturedNew = 0, capturedRef = 0;
  for ( int i = 0; i < nreq; ++i )
  {
    capturedNew += readNew[i];
    capturedRef += readRef[i];
  }
  if ( capturedNew != capturedRef )
  {
    printf("FAIL %s: total captured new=%zu ref=%zu\n",
           name, capturedNew, capturedRef);
    ok = 0;
  }
  if ( ok && capturedNew != 0 && memcmp(capNew, capRef, capturedNew) != 0 )
  {
    // Locate first differing byte for diagnostics.
    size_t d = 0;
    while ( d < capturedNew && capNew[d] == capRef[d] )
    {
      ++d;
    }
    printf("FAIL %s: output differs at byte %zu (new=%02X ref=%02X)\n",
           name, d, capNew[d], capRef[d]);
    ok = 0;
  }
  CHECK( ok, "%s: outputs differ between pool reader and realloc reader",
         name );

  int posNew = encPacket_position(newReader);
  int posRef = refPacket_position(refReader);
  CHECK( posNew == posRef, "%s: position new=%d ref=%d", name, posNew,
         posRef );

  encPacket_delete(&newReader);
  refPacket_delete(&refReader);
  CHECK( newReader == NULL && refReader == NULL, "%s: delete nulled", name );

  free(capNew);
  free(capRef);
  free(readNew);
  free(readRef);
  return 0;
}

static void testReaderDifferential(void)
{
  printf("== reader: differential (new pool vs old realloc) ==\n");

  // Fill src.
  for ( size_t i = 0; i < SRC_SIZE; ++i )
  {
    g_src[i] = (uint8_t)(nextRand() & 0xFF);
  }
  for ( int i = 0; i < 32; ++i )
  {
    g_key[i] = (uint8_t)(i + 1);
  }
  for ( int i = 0; i < 8; ++i )
  {
    g_nonce[i] = (uint8_t)(0x60 + i);
  }

  // 1. Index parsing pattern from hfs.c:
  //    per file: 4, 2*len, 4, 4, 4, 4, 4, 8, 4, 4
  {
    uint32_t reqs[64 * 10];
    int      n = 0;
    for ( int f = 0; f < 64; ++f )
    {
      uint32_t len = (uint32_t)(1 + (f * 7) % 255);
      reqs[n++] = 4;
      reqs[n++] = len * 2;
      reqs[n++] = 4;
      reqs[n++] = 4;
      reqs[n++] = 4;
      reqs[n++] = 4;
      reqs[n++] = 4;
      reqs[n++] = 8;
      reqs[n++] = 4;
      reqs[n++] = 4;
    }
    CHECK( runScenario("index-pattern", reqs, n, 0) == 0,
           "index-pattern failed" );
  }

  // 2. Exact chunk boundary reads: each 64-byte read lands exactly on the
  //    chunk end, which triggers the protocol's counter=0 reset. Both
  //    readers must agree on the (deliberately weird) byte stream.
  {
    uint32_t reqs[16];
    for ( int i = 0; i < 16; ++i )
    {
      reqs[i] = 64;
    }
    CHECK( runScenario("chunk-boundary-64", reqs, 16, 0) == 0,
           "chunk-boundary-64 failed" );
  }

  // 3. Off-by-one sizes around ChaCha20 blocks.
  {
    uint32_t reqs[] = { 1, 63, 64, 65, 127, 128, 129, 255, 256, 257,
                        511, 512, 1023, 1024, 4095, 4096, 4097 };
    CHECK( runScenario("block-edges", reqs, (int)(sizeof(reqs) / 4), 0) == 0,
           "block-edges failed" );
  }

  // 4. One large read (single 128 KiB chunk).
  {
    uint32_t reqs[] = { 131072 };
    CHECK( runScenario("single-large", reqs, 1, 0) == 0, "single-large failed" );
  }

  // 5. Many tiny reads (old code did one realloc each; worst case).
  {
    const int    N = 100000;
    uint32_t    *reqs = (uint32_t *)malloc(sizeof(uint32_t) * N);
    for ( int i = 0; i < N; ++i )
    {
      reqs[i] = 1;
    }
    CHECK( runScenario("many-1-byte", reqs, N, 0) == 0, "many-1-byte failed" );
    free(reqs);
  }

  // 6. Randomized mixed sizes, seeded so the run is reproducible.
  {
    const int    N = 1500;
    uint32_t    *reqs = (uint32_t *)malloc(sizeof(uint32_t) * N);
    size_t       total = 0;
    for ( int i = 0; i < N; ++i )
    {
      uint32_t r = (uint32_t)(nextRand() % 4096) + 1;
      // Bias some reads toward exact chunk boundaries to exercise the
      // counter-reset protocol path.
      if ( (nextRand() & 0xF) == 0 )
      {
        r = (r / 64) * 64 + 64;
      }
      reqs[i] = r;
      total += r;
    }
    CHECK( total <= 8 * 1024 * 1024, "random scenario too large" );
    CHECK( runScenario("random-mixed", reqs, N, 0) == 0,
           "random-mixed failed" );
    free(reqs);
  }

  // 7. Non-zero starting counter.
  {
    uint32_t reqs[] = { 100, 1, 200, 7, 3000 };
    CHECK( runScenario("start-counter-42", reqs, 5, 42) == 0,
           "start-counter-42 failed" );
  }
}

// ---------------------------------------------------------------------------
// 3. New-behavior checks (graceful exhaustion, API sanity)
// ---------------------------------------------------------------------------

static void testReaderEdgeCases(void)
{
  printf("== reader: edge cases ==\n");

  CHECK( encPacket_init(NULL, 0, g_nonce, g_key) == NULL,
         "init(NULL) must fail" );
  CHECK( encPacket_init(g_src, 0, NULL, g_key) == NULL, "init(NULL nonce)" );
  CHECK( encPacket_init(g_src, 0, g_nonce, NULL) == NULL, "init(NULL key)" );

  void *r = encPacket_init(g_src, 0, g_nonce, g_key);
  CHECK( r != NULL, "init failed" );

  uint8_t small[8];
  CHECK( encPacket_read(r, NULL, 8) == 0, "read(NULL dest) must be 0" );
  CHECK( encPacket_read(NULL, small, 8) == 0, "read(NULL reader) must be 0" );

  int got = encPacket_read(r, small, 8);
  CHECK( got == 8, "read(8) got %d", got );

  // Request far beyond the internal arena: must fail gracefully (partial/
  // zero read) instead of crashing, and the reader must stay usable.
  uint32_t huge    = (uint32_t)(2 * 1024 * 1024);
  int      gotHuge = -1;
  {
    uint8_t *tmp = (uint8_t *)malloc(huge);
    if ( tmp )
    {
      gotHuge = encPacket_read(r, tmp, huge);
      free(tmp);
    }
  }
  CHECK( gotHuge >= 0 && gotHuge < huge,
         "huge read unexpectedly fully succeeded (%d)", gotHuge );

  int got2 = encPacket_read(r, small, 8);
  CHECK( got2 == 8, "reader unusable after huge read (%d)", got2 );

  encPacket_delete(&r);
  encPacket_delete(&r); // second delete must be a no-op
  CHECK( r == NULL, "delete nulled" );
}

int main(void)
{
  setvbuf(stdout, NULL, _IONBF, 0); // flush on crash

  // Initialize shared keys before reader tests.
  for ( int i = 0; i < 32; ++i )
  {
    g_key[i] = (uint8_t)(i + 1);
  }
  for ( int i = 0; i < 8; ++i )
  {
    g_nonce[i] = (uint8_t)(0x60 + i);
  }
  for ( size_t i = 0; i < SRC_SIZE; ++i )
  {
    g_src[i] = (uint8_t)(nextRand() & 0xFF);
  }

  testPoolBasics();
  testReaderEdgeCases();
  testReaderDifferential();

  printf("\n%d checks, %d failures\n", g_checks, g_failures);
  return (g_failures == 0) ? 0 : 1;
}
