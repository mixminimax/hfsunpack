#ifndef HFS_INFLATE_H
#define HFS_INFLATE_H

#include <stddef.h>

/*
 * hfs_inflate() decompresses a zlib (RFC 1950) stream.
 *
 * This is a small backend-agnostic wrapper over libdeflate or zlib. Which
 * backend is used is decided at build time: CMake renders config.h from
 * config.h.in, and inflate.c dispatches on the macro defined there.
 * Callers only ever use this interface.
 */

/**
 * @brief Decompress a zlib (RFC 1950) stream into a buffer.
 *
 * @param src     Compressed data (must not be NULL).
 * @param src_len Length of @src in bytes.
 * @param dst     Destination buffer (must not be NULL).
 * @param dst_cap Size of @dst in bytes.
 * @param out_len Optional. On success, receives the number of bytes written
 *                to @dst.
 *
 * @return 0 on success, non-zero on failure.
 */
int hfs_inflate(const void *src,
                size_t      src_len,
                void       *dst,
                size_t      dst_cap,
                size_t     *out_len);

#endif // HFS_INFLATE_H
