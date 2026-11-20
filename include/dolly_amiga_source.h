#ifndef DOLLY_AMIGA_SOURCE_H
#define DOLLY_AMIGA_SOURCE_H
/* Experimental native backend, READ ONLY. Requires AmigaOS NDK. */
#include "dolly_block_source.h"
#include "dolly_dma_policy.h"
#include <stdint.h>
/* Report every DMA-related refusal to caller; CLI/GadTools must display it.
 * Callback receives a stable reason enum; detailed troubleshooting text
 * is available from dolly_dma_get_advice(reason, language).
 * Message is borrowed static text, valid during the callback. */
typedef void (*DollyAmigaDmaReportFn)(DollyDmaStatus reason,
                                     const char *message,void *user);
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
    DollyAmigaDmaReportFn report_dma_problem; /* required to open raw device */
    void *report_user;
    uint64_t required_read_end; /* exclusive byte offset; zero skips preflight */
    uint8_t verified_uae_ide;    /* allow READ(10) on the qualified UAE-IDE path */
    uint32_t buffer_memory;    /* validated AllocMem memory-type flags */
    uint8_t require_rom_scsi;   /* verify the actual Kickstart driver and use Chip RAM */
    uint8_t bypass_legacy_scsi_limit; /* still require an advertised large-offset command */
} DollyAmigaDeviceOptions;
/* Identify the opened scsi.device before using a stock Kickstart profile. */
int dolly_amiga_probe_rom_scsi(uint32_t unit);
struct IOStdReq;
/* Inspect the opened device's NSD command list. This does not write. */
int dolly_amiga_device_advertises_write(struct IOStdReq *request);
int dolly_amiga_device_advertises_nsd64_write(struct IOStdReq *request);
/* Read-only. Uses advertised NSD READ64 or the qualified UAE-IDE fallback
 * for large offsets. Other controllers need independent DMA verification. */
DollyBlockStatus dolly_amiga_open_device(const DollyAmigaDeviceOptions *opts,
                                        DollyBlockSource **out);
#endif
