#ifndef DOLLY_AMIGA_SOURCE_H
#define DOLLY_AMIGA_SOURCE_H
/* Experimental native backend, READ ONLY. Requires AmigaOS NDK. */
#include "dolly_block_source.h"
#include <stdint.h>
typedef struct DollyAmigaDeviceOptions {
    const char *device_name;
    uint32_t unit;
    uint64_t capacity_bytes;     /* MUST be independently established */
    uint32_t physical_block_bytes;
    uint32_t max_transfer_bytes; /* conservative, multiple of block size */
    uint32_t dma_address_mask;   /* verified controller/mount start-address mask */
    uint32_t dma_alignment;      /* required alignment: power of 2 >= 2 */
    uint8_t dma_profile_verified;/* fail closed unless explicitly set to 1 */
    uint8_t require_24bit_dma;   /* request MEMF_24BITDMA when required */
} DollyAmigaDeviceOptions;
/* No writes, no guessing size, no TD64 unless NSD capability is verified.
 * NDK compilation and actual-device operation remain UNVERIFIED. */
DollyBlockStatus dolly_amiga_open_device(const DollyAmigaDeviceOptions *opts,
                                        DollyBlockSource **out);
#endif
