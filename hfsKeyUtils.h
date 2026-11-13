#ifndef HFSKEYUTILS_E0D6717C_A556_4588_84F2_458D427C9776_H
#define HFSKEYUTILS_E0D6717C_A556_4588_84F2_458D427C9776_H

#include <stdint.h>

int hfs_GenerateArchiveHeaderKeyW(const wchar_t *path, char key[0x20]);
int hfs_GenerateArchiveIndexKeyW(const wchar_t *path,
                                 const void    *head,
                                 const void    *maskedKey,
                                 char           key[0x20],
                                 char          *keyResult,
                                 int            keyLength);
#endif // HFSKEYUTILS_E0D6717C_A556_4588_84F2_458D427C9776_H
