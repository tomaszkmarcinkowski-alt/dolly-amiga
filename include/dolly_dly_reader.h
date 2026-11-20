#ifndef DOLLY_DLY_READER_H
#define DOLLY_DLY_READER_H

#include "dolly_block_source.h"
#include "dolly_dly_set.h"

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

/* Header-only probe. The caller retains ownership of the container. */
DollyBlockStatus dolly_dly_probe_header(DollyBlockSource *container,DollyDlyImageInfo *info);
/* Same ownership as dolly_dly_open. Indexes chunks on demand and is intended
 * for bounded sequential validation before writing to a destination. */
DollyBlockStatus dolly_dly_open_lazy(DollyBlockSource *container,DollyBlockSource **out,
                                   DollyDlyImageInfo *info);

#define DOLLY_DLY_SET_MAGIC "Dolly split image 1\n"

/* Read an optional embedded manifest from the FORM prefix. */
DollyBlockStatus dolly_dly_set_unpack(const unsigned char *prefix,size_t bytes,
                                     DollyDlySetInfo *out);

/* Parse the canonical ASCII manifest written by Create Image. */
DollyBlockStatus dolly_dly_set_parse(const char *text,size_t bytes,DollyDlySetInfo *out);
int dolly_dly_set_matches(const DollyDlySetInfo *set,uint32_t index,
                          const DollyDlyImageInfo *part);

#endif
