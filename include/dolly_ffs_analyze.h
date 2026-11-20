#ifndef DOLLY_FFS_ANALYZE_H
#define DOLLY_FFS_ANALYZE_H
#include "dolly_block_source.h"

typedef enum DollyFfsRootPlacement {
    DOLLY_FFS_ROOT_UNCHANGED,
    DOLLY_FFS_ROOT_FREE,
    DOLLY_FFS_ROOT_NEW_SPACE,
    DOLLY_FFS_ROOT_METADATA,
    DOLLY_FFS_ROOT_OCCUPIED
} DollyFfsRootPlacement;

typedef struct DollyFfsReport {
    uint64_t blocks,used,copy_runs,highest_used,minimum_blocks,root_block;
    uint64_t target_blocks,used_outside_target,first_outside_target;
    uint64_t grow_root_block,grow_added_blocks,grow_tail_available;
    uint32_t grow_bitmap_pages,grow_bitmap_extensions;
    DollyFfsRootPlacement grow_root_placement;
    uint32_t block_bytes,reserved_blocks,dos_type;
} DollyFfsReport;

/* The caller supplies a stable, verified FFS partition extent. Positions in
 * the report are filesystem blocks relative to the start of the partition.
 * copy_runs counts contiguous allocated ranges, including reserved blocks.
 * NULL means success. A successful report is a bitmap estimate for planning,
 * and never authorizes a resize or certifies the filesystem tree. */
const char *dolly_ffs_analyze(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,
                              uint32_t expected_dos_type,uint64_t target_bytes,
                              DollyFfsReport *out);
/* Map an additional bitmap page or extension to its block in the added
 * range. Blocks are assigned in order, bitmap pages first. Returns 0 if
 * the requested placement cannot be made entirely in the added range. */
int dolly_ffs_grow_metadata_block(const DollyFfsReport *report,int extension,
                                  uint32_t index,uint64_t *block);
/* Build one page in memory. Existing pages require their original bytes;
 * a new page uses NULL. `output` may be the same buffer as `original`.
 * An occupied root destination must be resolved before building pages. */
int dolly_ffs_grow_bitmap_page(const DollyFfsReport *report,uint32_t index,
                              const unsigned char *original,unsigned char *output);
#endif
