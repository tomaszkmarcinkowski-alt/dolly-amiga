#ifndef DOLLY_DLY_READER_H
#define DOLLY_DLY_READER_H

#include "dolly_block_source.h"

typedef struct DollyDlyImageInfo {
    uint64_t first_lba, sectors;
    uint32_t sector_bytes, batch_bytes, version;
} DollyDlyImageInfo;

/* Opens a byte-addressed view of the decoded sectors starting at offset 0.
 * Takes ownership of container only on success. The caller supplies a stable
 * source for the lifetime of the view. Metadata and coverage are checked on
 * open. The CRC of each data chunk is checked before a read reports its bytes
 * as valid. Bytes beyond the reported read count must be ignored. */
DollyBlockStatus dolly_dly_open(DollyBlockSource *container, DollyBlockSource **out,
                               DollyDlyImageInfo *info);

#endif
