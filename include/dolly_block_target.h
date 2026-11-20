#ifndef DOLLY_BLOCK_TARGET_H
#define DOLLY_BLOCK_TARGET_H
#include "dolly_block_source.h"

/* The adapter owns the destination and supplies write, flush and, when
 * requested, readback. All callbacks use absolute offsets on the same
 * exclusively held medium. */
typedef struct DollyBlockTarget {
    uint64_t capacity;
    uint64_t start;
    uint64_t length;
    uint32_t sector_bytes;
    uint32_t max_transfer;
    void *context;
    DollyBlockStatus (*write)(void *context,uint64_t offset,
                              const void *data,size_t bytes,size_t *written);
    DollyBlockStatus (*flush)(void *context);
    DollyBlockReadFn readback;
    int supports_large_offsets;
} DollyBlockTarget;

typedef enum DollyBlockWriteStatus {
    DOLLY_WRITE_VERIFIED=0,
    DOLLY_WRITE_REJECTED,
    DOLLY_WRITE_FAILED,
    DOLLY_WRITE_FLUSH_FAILED,
    DOLLY_WRITE_VERIFY_FAILED,
    DOLLY_WRITE_MISMATCH,
    DOLLY_WRITE_COMPLETE
} DollyBlockWriteStatus;

/* Submit one complete, sector-aligned batch at a target-relative offset.
 * A failure after submission can leave modified sectors. Each caller must
 * stop the operation on failure and report it without retrying blindly.
 * With verify enabled, scratch must be at least bytes long and separate
 * from the input data. On success, returns DOLLY_WRITE_VERIFIED when
 * verify is enabled and DOLLY_WRITE_COMPLETE otherwise. Both modes flush. */
DollyBlockWriteStatus dolly_block_write(const DollyBlockTarget *target,
                      uint64_t offset,const void *data,size_t bytes,
                      int verify,void *scratch,size_t scratch_bytes);

/* Entry point for callers that always require readback. */
DollyBlockWriteStatus dolly_block_write_verified(const DollyBlockTarget *target,
                      uint64_t offset,const void *data,size_t bytes,
                      void *scratch,size_t scratch_bytes);
#endif
