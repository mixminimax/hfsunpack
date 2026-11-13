#include "hfsUtils.h"

// 0x5A6C0
size_t hfs_calcHeaderOffset(const wchar_t *path)
{
  // TODO: If file is not in hfs directory or not 256bit name, return 30
  if ( !path )
  {
    return 30;
  }

  wchar_t buffer[512] = {0};
  wcscpy(buffer, fileNameFromPath(path));
  wcharToLower(buffer);

  wchar_t *ptr   = buffer;
  int      value = 0;
  while ( *ptr )
  {
    value += *ptr;
    ptr++;
  }
  return value % 312 + 30;
}

// Distance from the header
size_t hfs_calcIndexOffset(const wchar_t *path)
{
  int     sum         = 0;
  wchar_t buffer[512] = {0};
  wcscpy(buffer, fileNameFromPath(path));
  wcharToLower(buffer);

  wchar_t *ptr = buffer;
  while ( *ptr )
  {
    sum += 3 * *(ptr++);
  }

  return sum % 0xD4 + 64 + 8;
}

const wchar_t *fileNameFromPath(const wchar_t *path)
{
  const wchar_t *ptr = path;
  ptr += wcslen(path);
  while ( ptr != path )
  {
    switch ( *ptr )
    {
    case L'/':
    case L'\\':
      return ptr + 1;
      break;
    }
    ptr--;
  }
  return ptr;
}

size_t hfs_calcPayloadOffset(const wchar_t *path, int indexSize)
{
  size_t sum = 0;
  sum        = hfs_calcIndexOffset(path);
  sum += indexSize; // Index size
  if ( (sum & 0x3FF) )
  {
    sum = (sum & 0xFFFFFC00) + 1024;
  }

  return sum;
}

void wcharToLower(wchar_t *target)
{
  while ( *target )
  {
    *target = towlower(*target);
    target++;
  }
}
#include <stdio.h>

const char *getIndexTableEncryptionKey(int8_t       *result,
                                       const int8_t *header,
                                       const int8_t *maskedKey,
                                       int           length)
{
  if ( !result || !header || !maskedKey )
  {
    return NULL;
  }

  for ( int i = 0; i < length; ++i )
  {
    result[i] = ((header[i]) >> 1) ^ maskedKey[i * 2] ^ 0x4B;
  }
  return result;
}
