#ifndef DOLLY_IO_PLAN_H
#define DOLLY_IO_PLAN_H
/* Portable planning of a single Amiga device read. No device access. */
#include "dolly_block_source.h"
#include <stdint.h>
#include <stddef.h>
#define DOLLY_CMD_READ UINT16_C(2)
#define DOLLY_NSCMD_TD_READ64 UINT16_C(0xc000)
#define DOLLY_NSCMD_TD_WRITE64 UINT16_C(0xc001)
typedef struct DollyIoReadPlan {
    uint16_t command;
    uint32_t offset_low;
    uint32_t offset_high;
    uint32_t byte_count;
} DollyIoReadPlan;
/* All offset/length arguments must be multiples of physical block_bytes.
 * The caller must have CONFIRMED NSD64 support before passing nsd64=1.
 * Legacy reads crossing 4 GiB are rejected, never wrapped or truncated.
 */
DollyBlockStatus dolly_io_plan_read(uint64_t offset, size_t requested,
                                   uint32_t block_bytes, uint32_t max_transfer,
                                   int nsd64, DollyIoReadPlan *out);
#endif
