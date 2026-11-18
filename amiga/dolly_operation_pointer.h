#ifndef DOLLY_OPERATION_POINTER_H
#define DOLLY_OPERATION_POINTER_H

#include "dolly_amiga_image.h"

struct Window;
/* Offer a checkpoint for an interrupted live scan or image. For imaging,
 * a successful save transfers ownership of the staging drawer to the
 * checkpoint. The caller still closes/cancels the operation afterwards. */
void dolly_operation_pointer_offer(struct Window *parent,const DollyEndpoint *source,
                         const DollyEndpoint *destination,DollySurfaceScan *scan,
                         DollyAmigaImage *image);
#endif
