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

typedef enum DollyFfsBootPayload {
    DOLLY_FFS_BOOT_UNCHECKED,
    DOLLY_FFS_BOOT_EMPTY,
    DOLLY_FFS_BOOT_NONZERO
} DollyFfsBootPayload;

typedef struct DollyFfsReport {
    uint64_t blocks,used,copy_runs,highest_used,minimum_blocks,root_block;
    uint64_t target_blocks,used_outside_target,first_outside_target;
    uint64_t grow_root_block,grow_added_blocks,grow_tail_available;
    uint32_t grow_bitmap_pages,grow_bitmap_extensions,grow_root_children;
    uint32_t grow_preview_bitmap_pages;
    uint64_t grow_preview_used,grow_preview_free;
    DollyFfsRootPlacement grow_root_placement;
    DollyFfsBootPayload grow_boot_payload;
    uint32_t shrink_root_block,shrink_bitmap_pages,shrink_bitmap_extensions;
    DollyFfsRootPlacement shrink_root_placement;
    uint32_t shrink_root_children,shrink_reclaimed_blocks,shrink_discarded_outside;
    DollyFfsBootPayload shrink_boot_payload;
    uint32_t shrink_required_pages_outside,shrink_required_extensions_outside;
    uint32_t shrink_first_required_bitmap_outside;
    uint64_t shrink_preview_free;
    int shrink_root_outside,shrink_root_reuses_bitmap,shrink_bitmap_preview_ready;
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
/* Progress reports the percentage of validated bitmap pages. */
typedef void (*DollyFfsProgress)(unsigned percent);
const char *dolly_ffs_analyze_with_progress(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,
                              uint32_t expected_dos_type,uint64_t target_bytes,
                              DollyFfsReport *out,DollyFfsProgress progress);
/* The visitor stages partition-relative byte ranges. Discard them if
 * validation fails, including when the visitor returns zero. */
typedef int (*DollyFfsRange)(void *context,uint64_t offset,uint64_t length);
const char *dolly_ffs_copy_plan(DollyBlockSource *source,uint64_t start,uint64_t length,
                              uint32_t block_bytes,uint32_t reserved,uint32_t dos_type,
                              DollyFfsReport *out,DollyFfsProgress progress,
                              DollyFfsRange range,void *context);
/* Patch a disposable, pre-sized partition copy. The callback receives
 * partition-relative byte offsets. Discard the copy on any failure.
 * Supports DOS/1 and DOS/3 with 512-byte blocks and an empty boot payload. */
typedef int (*DollyFfsPatch)(void *context,uint64_t offset,const void *data,size_t size);
const char *dolly_ffs_resize_copy(DollyBlockSource *source,uint64_t start,uint64_t length,
                                uint32_t reserved,uint64_t target_bytes,DollyFfsReport *out,
                                DollyFfsPatch patch,void *context);
/* Virtual resized partition backed by the source plus staged metadata.
 * The caller retains the source and must keep it open and isolated. */
const char *dolly_ffs_open_resized_source(DollyBlockSource *source,uint64_t start,uint64_t length,
                                        uint32_t reserved,
                                        uint64_t target,DollyFfsReport *report,
                                        DollyBlockSource **resized);
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
/* Prepare a relocated root in memory, retaining existing bitmap pointers.
 * Directory parent links are a separate step. */
int dolly_ffs_grow_root_preview(const DollyFfsReport *report,
                               const unsigned char *original,unsigned char *output);
/* Preview the two 512-byte boot sectors with the relocated root pointer.
 * Both buffers must hold 1024 bytes and may alias. Preserve boot checksum
 * validity without changing boot code. Other block sizes are unsupported. */
int dolly_ffs_grow_boot_preview(const DollyFfsReport *report,
                               const unsigned char *original,unsigned char *output);
/* Build a proposed bitmap extension. Indices start after the root's
 * inline bitmap references. Existing extensions require original bytes.
 * New extensions use NULL. Output may alias original. */
int dolly_ffs_grow_extension_preview(const DollyFfsReport *report,uint32_t index,
                                    const unsigned char *original,unsigned char *output);
#endif
