#include "hfs.h"

#include "dir.h"
#include "hfsKeyUtils.h"
#include "hfsUtils.h"
#include "inflate.h"
#include "monocypher.h"
#include "packetReader.h"
#include "utils.h"
#include "xcontentDecrypt.h"

#include <stdint.h>
#include <stdio.h>

// platform
#ifdef _WIN32
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <sys/mman.h>
#endif // _WIN32

struct HFSHeader
{
  uint32_t checksum;
  uint32_t fileCount;
};

struct FileIndex
{
  uint32_t length;         // Number of characters
  wchar_t  filename[256];  // Filepath
  uint32_t checksum;       // Unknown
  uint32_t flags;          // Flags Usually 7 (Compressed)
  uint32_t startBlock;     // Zero
  uint32_t size;           // Size of file
  uint32_t compressedSize; // Compressed Size
  uint32_t unk0[2];        // Unknown
  uint32_t fileKeySeed[4]; // Encryption key for file
  uint32_t unk1[2];        // Unknown
};

void keyMasker(uint8_t key[32])
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

static inline int openPackage(HFS *hfs, const char *path)
{
#ifdef _WIN32
// Throw to openPackageW()

// openPackageW(hfs, )
#else
  hfs->fileHandle = open(path, O_RDONLY);
  if ( hfs->fileHandle == -1 )
  {
    return -1;
  }

  struct stat sb;
  fstat(hfs->fileHandle, &sb);

  // POSIX doesn't use view
  hfs->fileView = NULL;

  hfs->fileMapping =
      mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, hfs->fileHandle, 0);
  if ( hfs->fileMapping == MAP_FAILED )
  {
    return -1;
  }

#endif // _WIN32
  return 0;
}

static inline int openPackageW(HFS *hfs, const wchar_t *path)
{
#ifdef _WIN32
  hfs->fileHandle = CreateFileW(
      path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if ( hfs->fileHandle == INVALID_HANDLE_VALUE )
  {
    return -1;
  }

  {
    LARGE_INTEGER la;
    GetFileSizeEx(hfs->fileHandle, &la);
    hfs->fileSize = la.QuadPart;
  }

  hfs->fileMapping =
      CreateFileMappingW(hfs->fileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
  if ( !hfs->fileMapping )
  {
    CloseHandle(hfs->fileHandle);
    return -2;
  }

  hfs->fileView = MapViewOfFile(hfs->fileMapping, FILE_MAP_READ, 0, 0, 0);
  if ( !hfs->fileView )
  {
    CloseHandle(hfs->fileMapping);
    CloseHandle(hfs->fileHandle);
    return -3;
  }
#else // POSIX

#endif // _WIN32
  return 1;
}

static inline void cleanupEverything(HFS *hfs)
{
  if ( hfs->fileView )
  {
    UnmapViewOfFile(hfs->fileView);
    hfs->fileView = NULL;
  }

  if ( hfs->fileView )
  {
    CloseHandle(hfs->fileMapping);
    hfs->fileMapping = NULL;
  }

  if ( hfs->fileHandle != INVALID_HANDLE_VALUE )
  {
    CloseHandle(hfs->fileHandle);
    hfs->fileHandle = INVALID_HANDLE_VALUE;
  }
}

static inline int getHfsInfo(HFS *hfs, const wchar_t *path)
{
  size_t headeroffset = hfs_calcHeaderOffset(path);
  size_t indexoffset  = hfs_calcIndexOffset(path);
  indexoffset += 0x1A + 4 + 1 + headeroffset;

  hfs->headeroffset = (int)headeroffset;
  hfs->indexoffset  = (int)indexoffset;

  int8_t *buffer = malloc(headeroffset);
  memcpy(buffer, hfs->fileView, headeroffset);

  int8_t type_xor = *(int8_t *)hfs->fileView;
  int8_t type     = *(((int8_t *)hfs->fileView) + headeroffset);
  type_xor >>= 1;
  type ^= type_xor;

  uint8_t keySize = *(((int8_t *)hfs->fileView) + headeroffset + 1);
  keySize ^= type_xor;
  keySize *= 2;

  if ( type != 4 )
  {
    cleanupEverything(hfs);
    return -1;
  }

  char keyForIndex[0x20];
  char key2[0x20];

  hfs_GenerateArchiveIndexKeyW(path,
                               hfs->fileView,
                               ((uint8_t *)hfs->fileView) + hfs->headeroffset +
                                   5,
                               keyForIndex,
                               key2,
                               32);

  strcpy_s(hfs->archiveKey, 16, key2);
  char nonce[0x8] = {0};

  struct HFSHeader header;
  {
    size_t      offset = headeroffset + keySize + 4 + 1;
    char        dest[0x40];
    char        key[0x20];
    char        nonce[0x8]   = {0};
    const char *maskedheader = ((int8_t *)hfs->fileView) + offset;
    hfs_GenerateArchiveHeaderKeyW(path, key);
    crypto_chacha20_djb(dest, maskedheader, 0x40, key, nonce, 0);

    memcpy(&header, dest, sizeof(header));

    hfs->numFiles  = header.fileCount;
    hfs->fileEntry = calloc(hfs->numFiles, sizeof(struct FileIndex));

    EncryptedPacketReader *reader = encPacket_init(
        ((int8_t *)hfs->fileView) + hfs->indexoffset, 0, nonce, keyForIndex);
    for ( int i = 0; i < hfs->numFiles; ++i )
    {
      struct FileIndex *index = &hfs->fileEntry[i];
      encPacket_read(reader, &index->length, 4);
      if ( index->length > 255 )
      {
        puts("ERROR!");
        break;
      }
      encPacket_read(reader, &index->filename, index->length * 2);
      encPacket_read(reader, &index->checksum, 4);
      encPacket_read(reader, &index->flags, 4);
      encPacket_read(reader, &index->startBlock, 4);
      encPacket_read(reader, &index->size, 4);
      encPacket_read(reader, &index->compressedSize, 4);
      encPacket_read(reader, index->unk0, 4);
      encPacket_read(reader, index->unk0 + 1, 4);
      encPacket_read(reader, index->fileKeySeed, sizeof(index->fileKeySeed));
      encPacket_read(reader, index->unk1, 4);
      encPacket_read(reader, index->unk1 + 1, 4);
    }
    hfs->payloadoffset = encPacket_position(reader);
    hfs->payloadoffset += hfs->indexoffset;
  }
  free(buffer);

  return 1;
}

HFS *hfs_open(const char *path)
{
#ifdef _WIN32
  struct HFS *hfs   = NULL;
  wchar_t    *wpath = NULL;
  int         req   = MultiByteToWideChar(CP_THREAD_ACP, 0, path, -1, NULL, 0);
  wpath             = malloc(req * sizeof(wchar_t));
  if ( wpath == NULL )
  {
    return NULL;
  }

  MultiByteToWideChar(CP_THREAD_ACP, 0, path, -1, wpath, req);
  hfs = hfs_openW(wpath);
  free(wpath);
  return hfs;

#else

  struct HFS hfs = {0};
  if ( !path )
  {
    return NULL;
  }

  // Setup for handles, map, etc
  if ( openPackage(&hfs, path) < 0 )
  {
    return NULL;
  }

  if ( getHfsInfo(&hfs, path) < 0 )
  {
    return NULL;
  }

  HFS *result = malloc(sizeof(HFS));
  memcpy(result, &hfs, sizeof(HFS));
  return result;
#endif // _WIN32
}

HFS *hfs_openW(const wchar_t *path)
{
  struct HFS hfs = {0};
  if ( !path )
  {
    return NULL;
  }

  // Setup for handles, map, etc
  if ( openPackageW(&hfs, path) < 0 )
  {
    return NULL;
  }

  if ( getHfsInfo(&hfs, path) < 0 )
  {
    return NULL;
  }

  HFS *result = malloc(sizeof(HFS));
  memcpy(result, &hfs, sizeof(HFS));
  return result;
}

void hfs_printFileLists(HFS *hfs)
{
  if ( !hfs )
  {
    return;
  }

  printf("Number of files: %d\n", hfs->numFiles);
  for ( int i = 0; i < hfs->numFiles; ++i )
  {
    if ( !hfs->fileEntry[i].filename[0] )
    {
      break;
    }
    printf("%02d %d %d: %ls\t(%d, %d)\n",
           i,
           hfs->fileEntry[i].flags,
           hfs->fileEntry[i].startBlock,
           hfs->fileEntry[i].filename,
           hfs->fileEntry[i].size,
           hfs->fileEntry[i].compressedSize);
  }
}

void hfs_close(HFS **hfs)
{
  if ( !hfs || !*hfs )
  {
    return;
  }

  free((*hfs)->fileEntry);

  cleanupEverything(*hfs);
  *hfs = NULL;
  return;
}

int hfs_extractFile(HFS *hfs, int fileIndex, wchar_t *path)
{
  if ( !hfs || !path )
  {
    return 0;
  }
  if ( fileIndex > hfs->numFiles - 1 )
  {
    return -1;
  }
  char secret[0x20]      = {0};
  char commonKey[0x20]   = {0};
  char nonce_counter[16] = {0};

  size_t offsetForCurrentFile = 0;

  size_t pathLength = wcsnlen(hfs->fileEntry[fileIndex].filename, 256);

#if 1
  for ( int i = 0; i < 32; ++i )
  {
    uint8_t pathChar =
        (uint8_t)hfs->fileEntry[fileIndex].filename[i % pathLength];
    // uint8_t pathChar = PATH[i % pathLength];
    uint8_t seedByte =
        ((uint8_t *)hfs->fileEntry[fileIndex].fileKeySeed)[i % 16];
    int n2   = i + 2;
    int step = 5 * (i / 5);

    commonKey[i] = (i + pathChar * (n2 + seedByte - step));
  }
#endif

  // Generate Nonce and Counter
  {
    // uint8_t seed[16]    = {0};
    uint8_t baseKey[16] = {0};
    // Generate Basekey
    {
      unsigned int hash = 0x811c9dc5u;
      char        *ptr  = hfs->archiveKey;
      while ( *ptr )
      {
        hash = (*ptr ^ hash);
        hash *= 0x1000193;
        ptr++;
      }

      int keys[4];
      {
        keys[0] = hash;
        keys[1] = hash >> 1;
        keys[2] = keys[1] ^ 0x6C;
        keys[3] = keys[2] << 2;
      }
      memcpy(&baseKey, keys, 16);
    }

    int v189 = 0;
    int n90  = 90;
    int v191 = 0;
    int v192 = 0;

    for ( int i = 0; i < 12; i++ )
    {
      uint8_t key =
          (uint8_t)(v192 + (11 * (i / 11)) + (n90 ^ (i >> 2)) + (v191 ^ v189));

      nonce_counter[i] = baseKey[i] ^ key;

      v192 -= 1;
      v191 += 8;
      v189 += 17;
      n90 += 43;
    }
  }

  memcpy(secret, commonKey, sizeof(commonKey));
  keyMasker(secret);

  // puts("Common Key - Shared for both secret, nonce and counter");
  // printinHex(commonKey, 0x20);
  // puts("Encryption Key");
  // printinHex(secret, 0x20);
  // puts("Nonce And Counter");
  // printinHex(nonce_counter, sizeof(nonce_counter));

  // Payload offset
  {
    size_t sum = 0;
    sum        = hfs->payloadoffset;
    if ( (sum & 0x3FF) )
    {
      // sum = (sum & 0xFFFFFC00) + 1024;
      sum = (sum + 1023) & ~1023;
    }

    offsetForCurrentFile = sum;
    offsetForCurrentFile += (hfs->fileEntry[fileIndex].startBlock * 0x400);
    printf("(%#zx) %ls:\n",
           offsetForCurrentFile,
           hfs->fileEntry[fileIndex].filename);
    // printf("Payload located: %#zx\t%d\t%#zx\n",
    //        sum,
    //        hfs->fileEntry[fileIndex].startBlock,
    //        offsetForCurrentFile);
  }

  uint64_t counter;
  memcpy(&counter, nonce_counter + 8, sizeof(counter));

  HANDLE test = CreateFileW_AutoDir(hfs->fileEntry[fileIndex].filename,
                                    GENERIC_WRITE,
                                    0,
                                    NULL,
                                    CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL,
                                    NULL);

  if ( test == INVALID_HANDLE_VALUE )
  {
    puts("Faild to create a destination file");
    return 0;
  }

  // File is compressed.
  if ( hfs->fileEntry[fileIndex].flags & 0x1 )
  {
    uint8_t *buffer = malloc(0x400);
    memcpy(buffer, ((uint8_t *)hfs->fileView) + offsetForCurrentFile, 0x400);
    crypto_chacha20_djb(buffer, buffer, 0x400, secret, nonce_counter, counter);
    size_t   compsize         = hfs->fileEntry[fileIndex].compressedSize;
    uint8_t *compressedbuffer = malloc(compsize < 0x400 ? 0x400 : compsize);
    memcpy(compressedbuffer, buffer, 0x400);
    if ( hfs->fileEntry[fileIndex].compressedSize > 0x400 )
    {
      memcpy(compressedbuffer + 0x400,
             ((uint8_t *)hfs->fileView) + offsetForCurrentFile + 0x400,
             hfs->fileEntry[fileIndex].compressedSize - 0x400);
    }

    void  *decompressed = malloc(hfs->fileEntry[fileIndex].size);
    size_t out          = 0;

    hfs_inflate(compressedbuffer,
                compsize,
                decompressed,
                hfs->fileEntry[fileIndex].size,
                &out);
    if ( _wcsnicmp(hfs->fileEntry[fileIndex].filename, L"scripts", 7) == 0 ||
         _wcsnicmp(hfs->fileEntry[fileIndex].filename, L"sqlite", 6) == 0 ||
         _wcsnicmp(hfs->fileEntry[fileIndex].filename,
                   L"resource/localized_text",
                   23) == 0 ||
         _wcsnicmp(hfs->fileEntry[fileIndex].filename,
                   L"resource\\localized_text",
                   23) == 0 )
    {
      xDecrypt(decompressed, hfs->fileEntry[fileIndex].size);
      WriteFile(test, ((char *)decompressed) + 1, (DWORD)out - 1, NULL, NULL);
    }
    else
    {
      WriteFile(test, decompressed, (DWORD)out, NULL, NULL);
    }
    free(decompressed);
    free(compressedbuffer);
    free(buffer);
  }
  else // Uncompressed
  {
    uint8_t *buffer =
        calloc(1, (hfs->fileEntry[fileIndex].size + 1023) & ~1023);

    memcpy(buffer,
           ((uint8_t *)hfs->fileView) + offsetForCurrentFile,
           hfs->fileEntry[fileIndex].compressedSize);

    if ( hfs->fileEntry[fileIndex].compressedSize ==
         hfs->fileEntry[fileIndex].size )
    {
      crypto_chacha20_djb(
          buffer, buffer, 0x400, secret, nonce_counter, counter);
    }
    else
    {
      crypto_chacha20_djb(buffer,
                          buffer,
                          hfs->fileEntry[fileIndex].size,
                          secret,
                          nonce_counter,
                          counter);
    }

    if ( _wcsnicmp(hfs->fileEntry[fileIndex].filename, L"scripts", 7) == 0 ||
         _wcsnicmp(hfs->fileEntry[fileIndex].filename, L"sqlite", 6) == 0 ||
         _wcsnicmp(hfs->fileEntry[fileIndex].filename,
                   L"resource/localized_text",
                   23) == 0 ||
         _wcsnicmp(hfs->fileEntry[fileIndex].filename,
                   L"resource\\localized_text",
                   23) == 0 )
    {
      char *encryptedbuffer = (char *)malloc(hfs->fileEntry[fileIndex].size);
      memcpy(encryptedbuffer, buffer, hfs->fileEntry[fileIndex].size);
      int sz = xDecrypt(encryptedbuffer, hfs->fileEntry[fileIndex].size);

      SetFilePointer(test, (LONG)sz, NULL, FILE_BEGIN);
      SetEndOfFile(test);
      SetFilePointer(test, 0, NULL, FILE_BEGIN);
      WriteFile(test, ((char *)encryptedbuffer) + 1, sz, NULL, NULL);
      free(encryptedbuffer);
    }
    else
    {
      WriteFile(test, buffer, hfs->fileEntry[fileIndex].size, NULL, NULL);
    }
    free(buffer);
  }

  CloseHandle(test);

  return 0;
}

int hfs_extractAll(HFS *hfs, wchar_t *path)
{
  for ( int i = 0; i < hfs->numFiles; ++i )
  {
    hfs_extractFile(hfs, i, path);
  }
  return hfs->numFiles;
}