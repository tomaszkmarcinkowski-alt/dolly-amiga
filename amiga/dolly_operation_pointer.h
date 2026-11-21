#ifndef DOLLY_OPERATION_POINTER_H
#define DOLLY_OPERATION_POINTER_H

#include "dolly_amiga_image.h"
#include "dolly_amiga_transfer.h"
#include "dolly_dlrp.h"

struct Window;
enum { DOLLY_RESUME_IMAGE=1,DOLLY_RESUME_TRANSFER };
int dolly_operation_pointer_pick(struct Window *parent,DollyDlrpImageResume *image,
                                  DollyDlrpTransferResume *transfer);
/* Offer a checkpoint for an interrupted live scan or image. For imaging,
 * a successful save transfers ownership of the staging drawer to the
 * checkpoint. The caller still closes/cancels the operation afterwards. */
void dolly_operation_pointer_offer(struct Window *parent,const DollyEndpoint *source,
                         const DollyEndpoint *destination,DollySurfaceScan *scan,
                         DollyAmigaImage *image);
void dolly_operation_pointer_offer_transfer(struct Window *parent,const DollyEndpoint *source,
                         const DollyEndpoint *destination,const DollyAmigaTransfer *transfer);
#endif
