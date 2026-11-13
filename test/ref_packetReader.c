#include "packetReader.h"

#include "monocypher.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Stream Packet Reader
struct RefPacketReader
{
  const uint8_t *src;
  uint64_t       srcOffset;
  uint8_t        nonce[8];
  uint8_t        key[32];
  uint64_t       counter;
  uint8_t       *buffer;
  int            bufferPosition;
  int            bufferSize;
};

void *refPacket_init(void    *src,
                     uint64_t counter,
                     uint8_t  nonce[0x8],
                     uint8_t  key[0x32])
{
  if ( !src )
  {
    return NULL;
  }

  struct RefPacketReader *result;
  result = calloc(1, sizeof(struct RefPacketReader));
  if ( !result )
  {
    return NULL;
  }
  result->src       = src;
  result->srcOffset = 0;
  memcpy(result->nonce, nonce, 8);
  memcpy(result->key, key, 32);
  result->counter = counter;

  return result;
}

void refPacket_delete(void **encPacket)
{
  if ( !encPacket || !*encPacket )
  {
    return;
  }
  struct RefPacketReader *pkt =
      (struct RefPacketReader *)*encPacket;
  if ( pkt->buffer )
  {
    free(pkt->buffer);
  }

  free(*encPacket);
  *encPacket = NULL;
}

int refPacket_read(struct RefPacketReader *reader,
                   void                         *dest,
                   const uint32_t                size)
{
  if ( !reader || !dest )
  {
    return 0;
  }

  uint8_t *destPtr        = (uint8_t *)dest;
  uint32_t bytesRemaining = size;
  uint32_t totalRead      = 0;
  // printf("Buffer Position: %d\n", reader->bufferPosition);

  while ( bytesRemaining > 0 )
  {
    // 1. Remain buffer
    int available = reader->bufferSize - reader->bufferPosition;

    if ( available > 0 )
    {
      // Remain?
      int copySize =
          (bytesRemaining < (uint32_t)available) ? bytesRemaining : available;

      int requestedNow = (int)bytesRemaining;

      memcpy(destPtr + totalRead,
             reader->buffer + reader->bufferPosition,
             copySize);

      reader->bufferPosition += copySize;
      totalRead += copySize;
      bytesRemaining -= copySize;
      if ( available == requestedNow )
      {
        reader->counter = 0;
      }
    }
    else
    {
      // 64 bytes align

      int stride = (bytesRemaining > 64) ? ((bytesRemaining + 63) & ~63) : 64;

      // TODO: current implementation continuously allocate buffer. Do something
      // else
      uint8_t *newBuffer = realloc(reader->buffer, stride);
      if ( !newBuffer )
      {
        return totalRead; // Return after read
      }

      reader->buffer         = newBuffer;
      reader->bufferSize     = stride;
      reader->bufferPosition = 0;

      // printf("READING %llX %llu\n", reader->srcOffset, reader->counter);

      crypto_chacha20_djb(reader->buffer,
                          reader->src + reader->srcOffset,
                          stride,
                          reader->key,
                          reader->nonce,
                          reader->counter);

      // 1 block == 64b
      reader->counter += (((uint32_t)stride) >> 6u);
      // reader->counter++;
      reader->srcOffset += stride;
    }
  }

  return totalRead;
}

int refPacket_position(struct RefPacketReader *reader)
{
  if ( !reader )
  {
    return -1;
  }
  return (int)reader->srcOffset;
}
