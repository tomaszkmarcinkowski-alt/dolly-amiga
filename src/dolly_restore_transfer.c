#include "dolly_restore_transfer.h"
#include "dolly_restore_source.h"

int dolly_restore_transfer_init(DollyRestoreTransfer *transfer,DollyBlockSource *source,
                      uint64_t target_bytes,uint32_t sector_bytes,uint32_t max_transfer,
                      void *target,DollyRestoreWriteBatch write_batch,int verify) {
    if(!source || dolly_block_size(source)!=target_bytes)return 0;
    return dolly_restore_transfer_init_prefix(transfer,source,target_bytes,sector_bytes,
                                              max_transfer,target,write_batch,verify);
}

int dolly_restore_transfer_init_prefix(DollyRestoreTransfer *transfer,DollyBlockSource *source,
                      uint64_t target_bytes,uint32_t sector_bytes,uint32_t max_transfer,
                      void *target,DollyRestoreWriteBatch write_batch,int verify) {
    uint64_t image_bytes=source?dolly_block_size(source):0;
    if(!transfer || !write_batch ||
       dolly_restore_check_raw(image_bytes,image_bytes,sector_bytes)!=DOLLY_BLOCK_OK ||
       dolly_restore_check_raw(target_bytes,target_bytes,sector_bytes)!=DOLLY_BLOCK_OK ||
       max_transfer<sector_bytes || max_transfer%sector_bytes)
        return 0;
    *transfer=(DollyRestoreTransfer){.source=source,.target=target,.write_batch=write_batch,
        .bytes=image_bytes<target_bytes?image_bytes:target_bytes,
        .batch_bytes=max_transfer,.verify=verify!=0,
        .state=DOLLY_RESTORE_TRANSFERRING};
    return 1;
}

DollyRestoreTransferState dolly_restore_transfer_step(DollyRestoreTransfer *transfer,
                                                     void *buffer,size_t buffer_bytes) {
    DollyBlockReadResult result;
    uint64_t remaining;
    size_t bytes;
    if(transfer->state!=DOLLY_RESTORE_TRANSFERRING)return transfer->state;
    remaining=transfer->bytes-transfer->processed;
    bytes=remaining<transfer->batch_bytes?(size_t)remaining:transfer->batch_bytes;
    if(!buffer || buffer_bytes<bytes) {
        transfer->read_status=DOLLY_BLOCK_EINVAL;
        return transfer->state=DOLLY_RESTORE_READ_FAILED;
    }
    result=dolly_block_read(transfer->source,transfer->processed,buffer,bytes);
    if(result.status!=DOLLY_BLOCK_OK || result.bytes_read!=bytes) {
        transfer->read_status=result.status;
        return transfer->state=DOLLY_RESTORE_READ_FAILED;
    }
    transfer->write_status=transfer->write_batch(transfer->target,transfer->processed,
                                                buffer,bytes,transfer->verify);
    if(transfer->write_status!=(transfer->verify?DOLLY_WRITE_VERIFIED:DOLLY_WRITE_COMPLETE))
        return transfer->state=DOLLY_RESTORE_WRITE_FAILED;
    transfer->processed+=bytes;
    if(transfer->processed==transfer->bytes)transfer->state=DOLLY_RESTORE_FINISHED;
    return transfer->state;
}
