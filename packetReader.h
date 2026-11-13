#ifndef PACKETREADER_C67E36E9_8D7B_4457_B66B_38A0933353ED_H
#define PACKETREADER_C67E36E9_8D7B_4457_B66B_38A0933353ED_H

#include <stdint.h>
#ifndef EncryptedPacketReader
typedef void EncryptedPacketReader;
#endif // EncryptedPacketReader

void *encPacket_init(void    *src,
                     uint64_t counter,
                     uint8_t  nonce[0x8],
                     uint8_t  key[0x32]);

void encPacket_delete(void **encPacket);

int encPacket_read(EncryptedPacketReader *reader,
                   void                  *dest,
                   const uint32_t         size);

int encPacket_position(EncryptedPacketReader *reader);

#endif // PACKETREADER_C67E36E9_8D7B_4457_B66B_38A0933353ED_H
