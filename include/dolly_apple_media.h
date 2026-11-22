#ifndef DOLLY_APPLE_MEDIA_H
#define DOLLY_APPLE_MEDIA_H
#include "dolly_block_source.h"

typedef enum DollyAppleMedia {
    DOLLY_APPLE_UNKNOWN,
    DOLLY_APPLE_HFS,
    DOLLY_APPLE_HFS_PLUS,
    DOLLY_APPLE_WRAPPED_HFS_PLUS,
    DOLLY_APPLE_HFSX,
    DOLLY_APPLE_APM,
    DOLLY_APPLE_INVALID,
    DOLLY_APPLE_UNSUPPORTED,
    DOLLY_APPLE_UNREADABLE
} DollyAppleMedia;

enum { DOLLY_APPLE_MAP_LIMIT=128 };

typedef struct DollyAppleVolume {
    uint64_t offset,bytes; /* Relative to the start of the supplied span. */
    uint64_t partition_bytes;
    uint32_t entry;        /* One-based APM entry number. */
    DollyAppleMedia media;
    char name[33];
} DollyAppleVolume;

typedef struct DollyAppleMap {
    unsigned count;
    DollyAppleVolume volumes[DOLLY_APPLE_MAP_LIMIT];
} DollyAppleMap;

/* Inspect a bounded 512-byte-sector span. Recognition validates basic
 * geometry and map extents; it does not check filesystem integrity. */
DollyAppleMedia dolly_apple_media_probe(DollyBlockSource *source,uint64_t start,uint64_t bytes);
/* Only verified HFS-family volumes are listed. A failed map publishes no entries. */
DollyAppleMedia dolly_apple_media_inspect(DollyBlockSource *source,uint64_t start,uint64_t bytes,
                                         DollyAppleMap *map);
const char *dolly_apple_media_name(DollyAppleMedia media);

int dolly_apple_media_is_volume(DollyAppleMedia media);
int dolly_shapeshifter_file_fits(uint64_t bytes);
int dolly_shapeshifter_partition_fits(uint64_t start,uint64_t bytes);
/* Classify the medium and check declared APM partition sizes, excluding
 * Apple_Free space. Also checks the size of a flat Macintosh volume. */
DollyAppleMedia dolly_shapeshifter_media_limits(DollyBlockSource *source,uint64_t start,
                                               uint64_t bytes,int *volume_too_large);

/* Build a read-only disk view from a flat HFS-family volume and a trusted
 * three-entry APM disk template. The view borrows both inputs until close.
 * NULL means success. The caller owns the returned view. */
const char *dolly_apple_disk_from_filedisk(DollyBlockSource *volume,DollyBlockSource *driver_template,
                                          DollyBlockSource **out);
#endif
