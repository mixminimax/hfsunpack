#define _CRT_SECURE_NO_WARNINGS
#include "hfsKeyUtils.h"

#include "hfsUtils.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void unk_5ba4a(uint8_t *key)
{
  // use for chacha key
  static const uint8_t basekey[32] = {
      0x7B, 0x2F, 0x35, 0x48, 0x43, 0x95, 0x02, 0xB9, 0xAE, 0x91, 0xA6,
      0xE1, 0xD8, 0xD6, 0x24, 0xB4, 0x33, 0x10, 0x1D, 0x3D, 0xC1, 0xBB,
      0xC6, 0xF4, 0xA5, 0xFE, 0xB3, 0x69, 0x6B, 0x56, 0xE4, 0x75,
  };
  if ( !key )
  {
    return;
  }

  for ( int i = 0; i < sizeof(basekey) / sizeof(*basekey); ++i )
  {
    // TODO: Convert wchar to byte
    key[i] ^= basekey[i];
  }
  return;
}

int hfs_GenerateArchiveHeaderKeyW(const wchar_t *path, char key[0x20])
{
  wchar_t buffer[512] = {0};
  wcscpy(buffer, fileNameFromPath(path));
  wcharToLower(buffer);

  uint8_t  index = 0;
  wchar_t *ptr   = buffer;
  while ( *ptr || index != 33 )
  {
    *ptr += index++;
    if ( index == 33 )
    {
      *ptr = L'0';
      break;
    }

    ptr++;
  }

  uint8_t u8key[32];
  for ( int i = 0; i < sizeof(u8key); ++i )
  {
    u8key[i] = (uint8_t)buffer[i];
  }

  unk_5ba4a(u8key);
  memcpy(key, u8key, sizeof(u8key));
  return 0;
}

int hfs_GenerateArchiveIndexKeyW(const wchar_t *path,
                                 const void    *head,
                                 const void    *maskedKey,
                                 char           key[0x20],
                                 char          *keyResult,
                                 int            keyLength)
{
  int     result = 0;
  wchar_t pathandstatickey[256];
  char    key2[32];
  wcscpy(pathandstatickey, fileNameFromPath(path));
  wcharToLower(pathandstatickey);
  {
    char    tmp[0x22]   = {0};
    wchar_t tmp_w[0x22] = {0};
    memset(tmp, 0, sizeof(tmp));
    memset(tmp_w, 0, sizeof(tmp_w));

    getIndexTableEncryptionKey(tmp, head, maskedKey, 0xD);

    mbstowcs(tmp_w, tmp, 0x20);
    wcscat(pathandstatickey, tmp_w);
    if ( keyResult && keyLength > strnlen(tmp, sizeof(tmp)) )
    {
      strncpy(keyResult, tmp, keyLength);
      result = strnlen(keyResult, 0x20);
    }
    else if ( !keyResult && keyLength <= 0 )
    {
      result = strnlen(tmp, sizeof(tmp));
    }
  }

  // Some funky operations
  // Takes 1 parameters:
  // (1) Lowered filename + Static key
  {
    wchar_t *ptr = pathandstatickey;
    while ( *ptr && *(ptr + 1) )
    {
      ptr++;
    }
    for ( int i = 0; i < 32; ++i )
    {
      int mul = (i % 3) + 2;

      // Result
      key2[i] = (uint8_t)(i + *ptr * mul);
      ptr--;
    }
  }

  static const uint8_t basekey[32] = {
      0x7B, 0x2F, 0x35, 0x48, 0x43, 0x95, 0x02, 0xB9, 0xAE, 0x91, 0xA6,
      0xE1, 0xD8, 0xD6, 0x24, 0xB4, 0x33, 0x10, 0x1D, 0x3D, 0xC1, 0xBB,
      0xC6, 0xF4, 0xA5, 0xFE, 0xB3, 0x69, 0x6B, 0x56, 0xE4, 0x75,
  };

  for ( int i = 0; i < sizeof(basekey) / sizeof(*basekey); ++i )
  {
    // TODO: Convert wchar to byte
    key2[i] ^= basekey[i];
  }
  memcpy(key, key2, sizeof(key2));

  return result;
}