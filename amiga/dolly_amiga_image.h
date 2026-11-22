#ifndef DOLLY_AMIGA_IMAGE_H
#define DOLLY_AMIGA_IMAGE_H
#include "dolly_amiga_surface_scan.h"
#include "dolly_dly_writer.h"
#include "dolly_dlrp.h"

struct DollyImageDrawer;

typedef struct DollyAmigaImage {
    DollySurfaceScan reader;
    DollyDlyWriter dly;
    BPTR drawer,file;
    struct Window *parent;
    struct DollyImageDrawer *drawers;
    int staging,keep_staging,split,format_dly,part_opened;
    uint32_t apple_entry; /* Zero for a complete source extent. */
    int shapeshifter;
    int resuming,resume_dirty,resume_header_dirty;
    void *resume_buffer;
    DollyBlockSource *resume_decoded;
    uint32_t resume_bytes;
    uint64_t resume_until,resume_checked;
    DollyDlyCodec codec;
    uint32_t parts,part_sectors;
    uint64_t part_start,part_end,set_id;
    uint64_t closed_bytes;
    void (*transfer_activity)(int active);
    char part_stem[256];
    char name[256],partial[261],staged_file[528];
    const char *failure;
    char read_failure[64];
} DollyAmigaImage;

/* Initialize to zero. Each step copies one sector-aligned chunk. */
int dolly_image_open(DollyAmigaImage *image,const DollyEndpoint *source,
                     const DollyEndpoint *target,struct Window *parent);
int dolly_image_source_requires_raw(const DollyEndpoint *source,struct Window *parent);
int dolly_image_resume_open(DollyAmigaImage *image,const DollyDlrpImageResume *resume,
                            struct Window *parent);
int dolly_image_resume_confirm(DollyAmigaImage *image,const DollyEndpoint *target);
void dolly_image_step(DollyAmigaImage *image);
void dolly_image_cancel(DollyAmigaImage *image);
#endif
