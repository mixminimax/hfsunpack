#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

/**
 * @brief Check if exists
 *
 * @param dirPath
 * @return BOOL
 */
static BOOL EnsureDirectoryExists(const wchar_t *dirPath)
{
  if ( !dirPath || dirPath[0] == L'\0' )
    return TRUE;

  // If found
  DWORD attrs = GetFileAttributesW(dirPath);
  if ( attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) )
  {
    return TRUE;
  }

  // extract parent path
  size_t   len        = wcslen(dirPath);
  wchar_t *parentPath = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
  if ( !parentPath )
    return FALSE;

  wcscpy(parentPath, dirPath);
  if ( len > 0 && parentPath[len - 1] == L'\\' )
  {
    parentPath[len - 1] = L'\0';
    len--;
  }

  wchar_t *lastSlash = wcsrchr(parentPath, L'\\');
  if ( lastSlash )
  {
    *lastSlash = L'\0';
    if ( !EnsureDirectoryExists(parentPath) )
    {
      free(parentPath);
      return FALSE;
    }
  }
  free(parentPath);

  // Create
  if ( !CreateDirectoryW(dirPath, NULL) )
  {
    if ( GetLastError() != ERROR_ALREADY_EXISTS )
    {
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * @brief CreateFileW Wrapper. But it automatically create missing path between
 *
 * @param lpFileName
 * @param dwDesiredAccess
 * @param dwShareMode
 * @param lpSecurityAttributes
 * @param dwCreationDisposition
 * @param dwFlagsAndAttributes
 * @param hTemplateFile
 * @return HANDLE
 */
HANDLE CreateFileW_AutoDir(const wchar_t        *lpFileName,
                           DWORD                 dwDesiredAccess,
                           DWORD                 dwShareMode,
                           LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                           DWORD                 dwCreationDisposition,
                           DWORD                 dwFlagsAndAttributes,
                           HANDLE                hTemplateFile)
{
  if ( !lpFileName )
  {
    SetLastError(ERROR_INVALID_PARAMETER);
    return INVALID_HANDLE_VALUE;
  }

  // normalize
  size_t   len            = wcslen(lpFileName);
  wchar_t *normalizedPath = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
  if ( !normalizedPath )
  {
    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
    return INVALID_HANDLE_VALUE;
  }

  wcscpy(normalizedPath, lpFileName);
  for ( size_t i = 0; i < len; i++ )
  {
    if ( normalizedPath[i] == L'/' )
    {
      normalizedPath[i] = L'\\';
    }
  }

  // extract path
  wchar_t *lastSlash = wcsrchr(normalizedPath, L'\\');
  if ( lastSlash )
  {
    *lastSlash = L'\0'; // Split by NULL
    if ( !EnsureDirectoryExists(normalizedPath) )
    {
      free(normalizedPath);
      return INVALID_HANDLE_VALUE; // Failed
    }
    *lastSlash = L'\\'; // Revert
  }

  // Finally
  HANDLE hFile = CreateFileW(normalizedPath,
                             dwDesiredAccess,
                             dwShareMode,
                             lpSecurityAttributes,
                             dwCreationDisposition,
                             dwFlagsAndAttributes,
                             hTemplateFile);

  free(normalizedPath);
  return hFile;
}
