#ifndef DIR_A50B3847_1517_4673_98E5_F72F242DA381_H
#define DIR_A50B3847_1517_4673_98E5_F72F242DA381_H
#include <windows.h>

HANDLE CreateFileW_AutoDir(LPCWSTR               lpFileName,
                           DWORD                 dwDesiredAccess,
                           DWORD                 dwShareMode,
                           LPSECURITY_ATTRIBUTES lpSecurityAttributes,
                           DWORD                 dwCreationDisposition,
                           DWORD                 dwFlagsAndAttributes,
                           HANDLE                hTemplateFile);

#endif // DIR_A50B3847_1517_4673_98E5_F72F242DA381_H
