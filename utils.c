#include "utils.h"

#include <stdint.h>
#include <stdio.h>

void printinHex(const void *_ptr, int length)
{
  uint8_t *ptr = (uint8_t *)_ptr;
  for ( int i = 0; i < length; ++i )
  {
    printf("%02X ", (uint8_t)ptr[i]);
    if ( !((i + 1) % 16) )
    {
      puts("");
    }
  }
}
