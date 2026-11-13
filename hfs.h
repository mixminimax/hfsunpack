#ifndef HFS_AAE836BC_5886_4651_B7AD_A55DCDBFF1EE_H
#define HFS_AAE836BC_5886_4651_B7AD_A55DCDBFF1EE_H

#include <wchar.h>

#ifndef HFS
// typedef void *HFS;
#endif // HFS

typedef struct HFS
{
  void             *fileHandle;
  void             *fileMapping;
  void             *fileView;
  size_t            fileSize;
  int               headeroffset;
  int               indexoffset;
  int               payloadoffset;
  int               numFiles;
  char              archiveKey[16];
  struct FileIndex *fileEntry;

} HFS;

// Open And Close

/**
 * @brief Open HFS Package
 *
 * @param path
 * @return HFS*
 */
HFS *hfs_open(const char *path);
HFS *hfs_openW(const wchar_t *path);

/**
 * @brief
 *
 * @param hfs
 */
void hfs_close(HFS **hfs);

// Archive access API. hfs Context must not NULL

/**
 * @brief
 *
 * @param hfs
 */
void hfs_printFileLists(HFS *hfs);

int hfs_numerOfFiles(HFS *hfs);

/**
 * @brief Return index of file. Index data will be copied to parameter "index"
 * hence, it is safe to do "hfs_close()" after this call. use free() after use.
 *
 * @param hfs Archive context
 * @param index Index pointer. Must be NULL. This pointer will be alloacted.
 * @return int
 */
int hfs_getFileIndex(HFS *hfs, struct FileIndex **index);

/**
 * @brief
 *
 * @param hfs
 * @param fileIndex
 * @param path
 * @return int
 */
int hfs_extractFile(HFS *hfs, int fileIndex, wchar_t *path);

/**
 * @brief
 *
 * @param hfs
 * @param path
 * @return int
 */
int hfs_extractAll(HFS *hfs, wchar_t *path);

#endif // HFS_AAE836BC_5886_4651_B7AD_A55DCDBFF1EE_H
