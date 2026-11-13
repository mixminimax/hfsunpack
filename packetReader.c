#include "packetReader.h"

#include "memoryPool.h"
#include "monocypher.h"
#include "utils.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Default arena size for decrypted chunks.
//
// The reader holds exactly one chunk (the current 64-byte aligned block) at a
// time and recycles the arena before every refill, so the arena only needs to
// be as large as the largest single encPacket_read() request. 1 MiB leaves a
// wide margin over the index sizes this project parses; it costs a single
// page-aligned reservation, not 1 MiB of live memory.
#define ENCPACKET_DEFAULT_POOL_SIZE (size_t)0x100000

// Stream Packet Reader
struct EncryptedPacketReader
{
  const uint8_t *src;
  uint64_t       srcOffset;
  uint8_t        nonce[8];
  uint8_t        key[32];
  uint64_t       counter;
  MemPool       *pool;   // Arena backing every decrypted chunk
  uint8_t       *buffer; // Current chunk (owned by pool)
  int            bufferPosition;
  int            bufferSize;
};

void *encPacket_init(void    *src,
                     uint64_t counter,
                     uint8_t  nonce[0x8],
                     uint8_t  key[0x32])
{
  if ( !src || !nonce || !key )
  {
    return NULL;
  }

  struct EncryptedPacketReader *result;
  result = calloc(1, sizeof(struct EncryptedPacketReader));
  if ( !result )
  {
    return NULL;
  }

  result->pool = memPool_init(ENCPACKET_DEFAULT_POOL_SIZE);
  if ( !result->pool )
  {
    free(result);
    return NULL;
  }

  result->src       = src;
  result->srcOffset = 0;
  memcpy(result->nonce, nonce, 8);
  memcpy(result->key, key, 32);
  result->counter = counter;

  return result;
}

void encPacket_delete(void **encPacket)
{
  if ( !encPacket || !*encPacket )
  {
    return;
  }
  struct EncryptedPacketReader *pkt =
      (struct EncryptedPacketReader *)*encPacket;

  memPool_destroy(pkt->pool);

  free(pkt);
  *encPacket = NULL;
}

int encPacket_read(struct EncryptedPacketReader *reader,
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

      // The previous chunk has been fully consumed, so recycle the arena and
      // carve out the next chunk as one bump allocation. This replaces the
      // old per-refill realloc() loop: no regrowth, no copying, no
      // fragmentation.
      memPool_reset(reader->pool);
      uint8_t *newBuffer =
          (uint8_t *)memPool_alloc(reader->pool, (size_t)stride);
      if ( !newBuffer )
      {
        reader->buffer         = NULL;
        reader->bufferSize     = 0;
        reader->bufferPosition = 0;
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

int encPacket_position(struct EncryptedPacketReader *reader)
{
  if ( !reader )
  {
    return -1;
  }
  return (int)reader->srcOffset;
}
