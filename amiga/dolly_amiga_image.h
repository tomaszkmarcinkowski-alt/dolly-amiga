#ifndef DOLLY_AMIGA_IMAGE_H
#define DOLLY_AMIGA_IMAGE_H
#include "dolly_amiga_surface_scan.h"
#include "dolly_dly_writer.h"

struct DollyImageDrawer;

typedef struct DollyAmigaImage {
    DollySurfaceScan reader;
    DollyDlyWriter dly;
    BPTR drawer,file;
    struct Window *parent;
    struct DollyImageDrawer *drawers;
    int staging,keep_staging,split,format_dly,part_opened;
    DollyDlyCodec codec;
    uint32_t parts,part_sectors;
    uint64_t part_start,part_end,set_id;
    char part_stem[256];
    char name[256],partial[261],staged_file[528];
    const char *failure;
    char read_failure[64];
} DollyAmigaImage;

/* Initialize to zero. Each step copies one sector-aligned chunk. */
int dolly_image_open(DollyAmigaImage *image,const DollyEndpoint *source,
                     const DollyEndpoint *target,struct Window *parent);
void dolly_image_step(DollyAmigaImage *image);
void dolly_image_cancel(DollyAmigaImage *image);
#endif
