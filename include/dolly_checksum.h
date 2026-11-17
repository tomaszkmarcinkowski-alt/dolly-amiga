#ifndef DOLLY_CHECKSUM_H
#define DOLLY_CHECKSUM_H
/* CRC-32/ISO-HDLC, streaming and allocation-free; accidental corruption
 * detection only. CRC32 NEVER authenticates code or plugin provenance. */
#include <stddef.h>
#include <stdint.h>
typedef struct DollyChecksum { uint32_t crc; uint64_t bytes; } DollyChecksum;
void dolly_checksum_init(DollyChecksum *state);
int dolly_checksum_update(DollyChecksum *state,const void *data,size_t size);
uint32_t dolly_checksum_final(const DollyChecksum *state);
#endif
