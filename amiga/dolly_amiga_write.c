#include "dolly_amiga_write.h"
#include "dolly_dma_policy.h"
#include "dolly_io_plan.h"
#include <string.h>
#ifdef DOLLY_AMIGA_MOCK
#include "amiga_mock_exec.h"
#else
#include <exec/io.h>
#include <proto/exec.h>
#endif

static DollyBlockStatus submit(struct IOStdReq *request,UWORD command,
                              uint64_t offset,void *data,size_t bytes) {
    request->io_Command=command;
    request->io_Flags=0;
    request->io_Error=0;
    request->io_Offset=(uint32_t)offset;
    request->io_Actual=(uint32_t)(offset>>32);
    request->io_Data=data;
    request->io_Length=(ULONG)bytes;
    if(DoIO((struct IORequest *)request) || request->io_Error)
        return DOLLY_BLOCK_EIO;
    return DOLLY_BLOCK_OK;
}

static DollyBlockStatus write_batch(void *opaque,uint64_t offset,const void *data,
                                   size_t bytes,size_t *written) {
    const DollyAmigaWriteTarget *target=opaque;
    DollyIoReadPlan plan;
    DollyBlockStatus status=dolly_io_plan_read(offset,bytes,target->sector_bytes,
                                       target->max_transfer,target->nsd64,&plan);
    if(status!=DOLLY_BLOCK_OK)return status;
    memcpy(target->bounce,data,bytes);
    status=submit(target->request,plan.command==DOLLY_NSCMD_TD_READ64 ?
                  DOLLY_NSCMD_TD_WRITE64 : CMD_WRITE,offset,target->bounce,bytes);
    *written=target->request->io_Actual;
    return status;
}

static DollyBlockStatus flush_batch(void *opaque) {
    const DollyAmigaWriteTarget *target=opaque;
    return submit(target->request,CMD_UPDATE,0,NULL,0);
}

DollyBlockReadResult dolly_amiga_write_target_read(const DollyAmigaWriteTarget *target,
                                       uint64_t offset,void *data,size_t bytes) {
    DollyBlockReadResult result={DOLLY_BLOCK_EINVAL,0};
    DollyIoReadPlan plan;
    if(!bytes || bytes>target->bounce_bytes || bytes>target->max_transfer ||
       offset>target->capacity || bytes>target->capacity-offset)return result;
    result.status=dolly_io_plan_read(offset,bytes,target->sector_bytes,
                                    target->max_transfer,target->nsd64,&plan);
    if(result.status!=DOLLY_BLOCK_OK)return result;
    result.status=submit(target->request,plan.command,offset,target->bounce,bytes);
    if(result.status!=DOLLY_BLOCK_OK)return result;
    if(target->request->io_Actual!=bytes) {
        result.status=DOLLY_BLOCK_ESHORT;
        return result;
    }
    if(data!=target->bounce)memcpy(data,target->bounce,bytes);
    result.bytes_read=bytes;
    return result;
}

static DollyBlockReadResult read_batch(void *opaque,uint64_t offset,void *data,
                                      size_t bytes) {
    const DollyAmigaWriteTarget *target=opaque;
    unsigned char *buffer=data;
    size_t i;
    /* The DMA buffer still contains the write data. A driver that reports
     * completion without updating it must not get a false match. */
    for(i=0;i<bytes;++i)buffer[i]^=0xffu;
    return dolly_amiga_write_target_read(target,offset,data,bytes);
}

DollyBlockWriteStatus dolly_amiga_write_batch(const DollyAmigaWriteTarget *target,
                       uint64_t offset,const void *data,size_t bytes,int verify) {
    DollyBlockTarget block;
    uintptr_t source=(uintptr_t)data,dma;
    uint64_t dma_address;
    if(!target || !target->request || !target->bounce || !data || !bytes ||
       bytes>target->bounce_bytes || target->max_transfer>65536u ||
       target->sector_bytes<512u || target->sector_bytes>32768u ||
       (target->sector_bytes&(target->sector_bytes-1u)))
        return DOLLY_WRITE_REJECTED;
    dma=(uintptr_t)target->bounce;
    if((source<dma?dma-source:source-dma)<bytes)return DOLLY_WRITE_REJECTED;
#ifdef DOLLY_AMIGA_MOCK
    dma_address=dolly_mock_dma_address();
#else
    dma_address=dma;
#endif
    if(dolly_dma_check_buffer(dma_address,bytes,target->dma_mask,
                              target->dma_alignment)!=DOLLY_DMA_OK)
        return DOLLY_WRITE_REJECTED;
    block=(DollyBlockTarget){
        target->capacity,target->start,target->length,target->sector_bytes,
        target->max_transfer,(void *)target,write_batch,flush_batch,read_batch,target->nsd64
    };
    return dolly_block_write(&block,offset,data,bytes,verify,
                             target->bounce,target->bounce_bytes);
}
