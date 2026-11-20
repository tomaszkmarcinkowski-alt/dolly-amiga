#ifndef DOLLY_AMIGA_RESTORE_SOURCE_H
#define DOLLY_AMIGA_RESTORE_SOURCE_H
#include "dolly_endpoint.h"
#include "dolly_restore_source.h"
#include <dos/dos.h>

/* Measure an open regular image file using classic DOS offsets.
 * A successful call leaves the file positioned at its beginning. */
DollyBlockStatus dolly_amiga_image_file_size(BPTR file,uint64_t *size);

struct Window;
struct ImageSet;

typedef struct DollyAmigaRestoreSource {
    DollyBlockSource *blocks;
    int is_dly;
    struct ImageSet *set;
    BPTR lock; /* Borrowed from the source. Kept until close; zero if unavailable. */
} DollyAmigaRestoreSource;

/* Initialize to zero. Close before reusing. Recognizes FORM DOLY by content
 * and split IFF DOLY manifests or opens flat RAW/HDF. The lock identifies
 * the actual opened file or the manifest on the parts' verified volume. */
DollyBlockStatus dolly_amiga_restore_source_open(DollyAmigaRestoreSource *source,
                         const DollyEndpoint *image,
                         uint32_t sector_bytes,struct Window *parent);
/* Enumerate distinct source volumes for the destination overlap guard. */
BPTR dolly_amiga_restore_volume(const DollyAmigaRestoreSource *source,uint32_t index);
uint32_t dolly_amiga_restore_failed_part(const DollyAmigaRestoreSource *source,uint32_t *total);
void dolly_amiga_restore_source_close(DollyAmigaRestoreSource *source);
#endif
