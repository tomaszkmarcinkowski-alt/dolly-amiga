#ifndef DOLLY_AMIGA_RESTORE_SOURCE_H
#define DOLLY_AMIGA_RESTORE_SOURCE_H
#include "dolly_endpoint.h"
#include "dolly_restore_source.h"
#include <dos/dos.h>

/* Measure an open regular image file using classic DOS offsets.
 * A successful call leaves the file positioned at its beginning. */
DollyBlockStatus dolly_amiga_image_file_size(BPTR file,uint64_t *size);

typedef struct DollyAmigaRestoreSource {
    DollyBlockSource *blocks;
    int is_dly;
    BPTR lock; /* Borrowed from the source. Kept until close; zero if unavailable. */
} DollyAmigaRestoreSource;

/* Initialize to zero. Close before reusing. Recognizes FORM DOLY by content
 * or opens flat RAW/HDF. The lock identifies the actual opened file. */
DollyBlockStatus dolly_amiga_restore_source_open(DollyAmigaRestoreSource *source,
                         const DollyEndpoint *image,
                         uint32_t sector_bytes);
void dolly_amiga_restore_source_close(DollyAmigaRestoreSource *source);
#endif
