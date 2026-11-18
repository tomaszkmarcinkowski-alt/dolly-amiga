#ifndef DOLLY_AMIGA_WRITE_H
#define DOLLY_AMIGA_WRITE_H

#include "dolly_block_target.h"

struct IOStdReq;

/* Borrowed resources. The caller must hold the destination exclusively,
 * revalidate its identity and extent, and qualify CMD_WRITE/CMD_UPDATE
 * and the DMA buffer for this driver before invoking the adapter. */
typedef struct DollyAmigaWriteTarget {
    struct IOStdReq *request;
    void *bounce;
    size_t bounce_bytes;
    uint64_t capacity,start,length;
    uint32_t sector_bytes,max_transfer;
    uint32_t dma_mask,dma_alignment;
} DollyAmigaWriteTarget;

/* Offset is relative to start. Data must not overlap the DMA buffer.
 * The caller must stop on any failure after submission. */
DollyBlockWriteStatus dolly_amiga_write_batch(const DollyAmigaWriteTarget *target,
                       uint64_t offset,const void *data,size_t bytes,int verify);
#endif
