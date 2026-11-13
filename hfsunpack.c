#define _CRT_SECURE_NO_WARNINGS

#include "hfs.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

int wmain(int argc, const wchar_t *argv[])
{
  int index = 0;
  if ( argc < 2 )
  {
    puts("no dir or file specified");
    return 0;
  }

  wchar_t path[512] = {0};
  wcscpy(path, argv[1]);
  wcscat(path, L"\\*.hfs");
  WIN32_FIND_DATAW pdata;
  HANDLE           hndl = FindFirstFileW(path, &pdata);
  if ( hndl == INVALID_HANDLE_VALUE )
  {
    return -1;
  }

  do
  {
    wcscpy(path, argv[1]);
    wcscat(path, L"\\");
    wcscat(path, pdata.cFileName);
    HFS *hfs = hfs_openW(path);
    if ( !hfs )
    {
      puts("hfs_open() failed");
      return -1;
    }

    printf("%ls\n", pdata.cFileName);
    hfs_printFileLists(hfs);
    // hfs_extractFile(hfs, 52, L"");
    hfs_extractAll(hfs, L"");
    puts("");

    hfs_close(&hfs);
  } while ( FindNextFileW(hndl, &pdata) );

  FindClose(hndl);
  return 0;
}
