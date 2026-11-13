#ifndef HFSUTILS_DB453E2E_3C8D_4081_A842_DDD59E6784F2_H
#define HFSUTILS_DB453E2E_3C8D_4081_A842_DDD59E6784F2_H

#include <stdint.h>
#include <string.h>
#include <wchar.h>

size_t hfs_calcHeaderOffset(const wchar_t *path);

size_t hfs_calcIndexOffset(const wchar_t *path);

size_t hfs_calcPayloadOffset(const wchar_t *path, int indexSize);

const wchar_t *fileNameFromPath(const wchar_t *path);

void wcharToLower(wchar_t *target);

/**
 * @brief Get the Index Table Encryption Key object
 *
 * @param result
 * @param header
 * @param maskedKey
 * @param length
 * @return const char*
 */
const char *getIndexTableEncryptionKey(int8_t       *result,
                                       const int8_t *header,
                                       const int8_t *maskedKey,
                                       int           length);

#endif // HFSUTILS_DB453E2E_3C8D_4081_A842_DDD59E6784F2_H
