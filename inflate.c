#include "inflate.h"

#include "config.h"

#if defined(HFS_INFLATE_WITH_LIBDEFLATE)

#  include "libdeflate.h"

int hfs_inflate(
    const void *src, size_t src_len, void *dst, size_t dst_cap, size_t *out_len)
{
  struct libdeflate_decompressor *decomp;
  size_t                          out = 0;

  if ( !src || !dst )
  {
    return -1;
  }

  decomp = libdeflate_alloc_decompressor();
  if ( !decomp )
  {
    return -1;
  }

  enum libdeflate_result result =
      libdeflate_zlib_decompress(decomp, src, src_len, dst, dst_cap, &out);
  libdeflate_free_decompressor(decomp);

  if ( result != LIBDEFLATE_SUCCESS )
  {
    return (int)result;
  }

  if ( out_len )
  {
    *out_len = out;
  }
  return 0;
}

#elif defined(HFS_INFLATE_WITH_ZLIB)

#  include <stdint.h>
#  include <string.h>
#  include <zlib.h>

int hfs_inflate(
    const void *src, size_t src_len, void *dst, size_t dst_cap, size_t *out_len)
{
  z_stream zs;
  int      ret;

  if ( !src || !dst )
  {
    return -1;
  }
  if ( src_len > (size_t)UINT_MAX || dst_cap > (size_t)UINT_MAX )
  {
    return -1;
  }

  memset(&zs, 0, sizeof(zs));
  zs.next_in   = (Bytef *)src;
  zs.avail_in  = (uInt)src_len;
  zs.next_out  = (Bytef *)dst;
  zs.avail_out = (uInt)dst_cap;

  if ( inflateInit(&zs) != Z_OK )
  {
    return -1;
  }

  ret = inflate(&zs, Z_FINISH);
  inflateEnd(&zs);

  // Z_STREAM_END: the stream was fully consumed.
  // Z_OK: the stream ended but the input had trailing bytes (lenient).
  if ( ret != Z_STREAM_END && ret != Z_OK )
  {
    return ret;
  }

  if ( out_len )
  {
    *out_len = (size_t)zs.total_out;
  }
  return 0;
}

#else
#  error "inflate.c: no decompression backend selected (see config.h.in)"
#endif
