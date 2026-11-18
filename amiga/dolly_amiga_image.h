#ifndef DOLLY_AMIGA_IMAGE_H
#define DOLLY_AMIGA_IMAGE_H
#include "dolly_amiga_surface_scan.h"
#include "dolly_dly_writer.h"

typedef struct DollyAmigaImage {
    DollySurfaceScan reader;
    DollyDlyWriter dly;
    BPTR drawer,file;
    int staging;
    char name[256],partial[261],staged_file[267];
    const char *failure;
} DollyAmigaImage;

/* Initialize to zero. Each step copies one sector-aligned chunk. */
int dolly_image_open(DollyAmigaImage *image,const DollyEndpoint *source,
                     const DollyEndpoint *target,struct Window *parent);
void dolly_image_step(DollyAmigaImage *image);
void dolly_image_cancel(DollyAmigaImage *image);
#endif
