#define _CRT_SECURE_NO_WARNINGS

#include "inflate.h"

#include "config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(HFS_INFLATE_WITH_LIBDEFLATE)
#  include "libdeflate.h"
#elif defined(HFS_INFLATE_WITH_ZLIB)
#  include <zlib.h>
#else
#  error "inflate_test: no decompression backend selected (see config.h.in)"
#endif

#define PLAIN_LEN 262144
#define COMP_CAP (PLAIN_LEN + 1024)

static int g_failures = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if ( cond ) {                                                              \
      printf("PASS: %s\n", (msg));                                             \
    }                                                                          \
    else {                                                                     \
      printf("FAIL: %s\n", (msg));                                             \
      ++g_failures;                                                            \
    }                                                                          \
  } while (0)

/*
 * Compress @plain with whichever backend is selected, so the test is
 * self-contained (no external compressed fixtures needed).
 */
static size_t compressBackend(const uint8_t *plain, size_t plain_len,
                              uint8_t *compbuf, size_t comp_cap)
{
#if defined(HFS_INFLATE_WITH_LIBDEFLATE)
  struct libdeflate_compressor *c = libdeflate_alloc_compressor(6);
  size_t                        out;
  if ( !c )
  {
    return 0;
  }
  out = libdeflate_zlib_compress(c, plain, plain_len, compbuf, comp_cap);
  libdeflate_free_compressor(c);
  return out;
#else
  z_stream zs;
  memset(&zs, 0, sizeof(zs));
  zs.next_in   = (Bytef *)plain;
  zs.avail_in  = (uInt)plain_len;
  zs.next_out  = (Bytef *)compbuf;
  zs.avail_out = (uInt)comp_cap;
  if ( deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK ||
       deflate(&zs, Z_FINISH) != Z_STREAM_END )
  {
    deflateEnd(&zs);
    return 0;
  }
  size_t out = (size_t)zs.total_out;
  deflateEnd(&zs);
  return out;
#endif
}

int main(void)
{
  uint8_t *plain = malloc(PLAIN_LEN);
  uint8_t *comp  = malloc(COMP_CAP);
  uint8_t *out   = malloc(PLAIN_LEN);
  size_t   complen = 0;
  size_t   outlen  = 0;
  int      rc;

  if ( !plain || !comp || !out )
  {
    return 2;
  }

  for ( size_t i = 0; i < PLAIN_LEN; ++i )
  {
    plain[i] = (uint8_t)((i * 31 + (i >> 5) ^ 0xA5) & 0xFF);
  }

  complen = compressBackend(plain, PLAIN_LEN, comp, COMP_CAP);
  if ( complen == 0 )
  {
    puts("FAIL: compression (test setup) failed");
    return 1;
  }

  // Round trip: decompress with the wrapper.
  rc = hfs_inflate(comp, complen, out, PLAIN_LEN, &outlen);
  CHECK(rc == 0, "hfs_inflate() succeeds on a valid zlib stream");
  CHECK(outlen == PLAIN_LEN, "decompressed length matches plain length");
  CHECK(memcmp(plain, out, PLAIN_LEN) == 0, "round-trip data matches");

  // Error handling.
  rc = hfs_inflate(NULL, complen, out, PLAIN_LEN, &outlen);
  CHECK(rc != 0, "hfs_inflate() rejects a NULL src");

  rc = hfs_inflate(comp, complen, NULL, PLAIN_LEN, &outlen);
  CHECK(rc != 0, "hfs_inflate() rejects a NULL dst");

  rc = hfs_inflate(comp, complen, out, 16, &outlen);
  CHECK(rc != 0, "hfs_inflate() rejects an undersized output buffer");

  {
    uint8_t garbage[4] = {0x01, 0x02, 0x03, 0x04}; // invalid zlib header
    rc                = hfs_inflate(garbage, sizeof(garbage), out, PLAIN_LEN,
                                    &outlen);
    CHECK(rc != 0, "hfs_inflate() rejects a corrupt stream");
  }

  free(plain);
  free(comp);
  free(out);

  if ( g_failures )
  {
    printf("%d test(s) FAILED\n", g_failures);
    return 1;
  }
  puts("all tests passed");
  return 0;
}
