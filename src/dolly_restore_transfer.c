#include "dolly_restore_transfer.h"
#include "dolly_restore_source.h"
#include <string.h>

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

int dolly_restore_transfer_set_ranges(DollyRestoreTransfer *transfer,
                                      const DollyTransferRange *ranges,size_t count,
                                      uint32_t sector_bytes) {
    size_t i;
    uint64_t end=0,total=0;
    if(!transfer || !ranges || !count || !sector_bytes || transfer->ranges ||
       transfer->state!=DOLLY_RESTORE_TRANSFERRING || transfer->processed ||
       transfer->source_check_bytes || transfer->resume_read ||
       transfer->batch_bytes%sector_bytes)return 0;
    for(i=0;i<count;++i) {
        const DollyTransferRange *r=&ranges[i];
        if(!r->length || r->offset<end || r->offset>transfer->bytes ||
           r->length>transfer->bytes-r->offset ||
           r->offset%sector_bytes || r->length%sector_bytes)return 0;
        end=r->offset+r->length;
        total+=r->length;
    }
    transfer->ranges=ranges;
    transfer->range_count=count;
    transfer->bytes=total;
    return 1;
}

int dolly_restore_transfer_set_resume(DollyRestoreTransfer *transfer,
                       uint64_t committed,uint64_t uncertain,uint32_t sector_bytes,
                       DollyBlockReadFn read_target,void *scratch,size_t scratch_bytes) {
    if(!transfer || !read_target || !scratch || !sector_bytes || !transfer->batch_bytes ||
       scratch_bytes<transfer->batch_bytes || transfer->resume_read || transfer->ranges ||
       transfer->processed || transfer->source_checked ||
       transfer->state!=DOLLY_RESTORE_TRANSFERRING || committed>=transfer->bytes ||
       committed%transfer->batch_bytes || uncertain>transfer->bytes-committed ||
       uncertain>transfer->batch_bytes || uncertain%sector_bytes)return 0;
    transfer->resume_committed=committed;
    transfer->resume_check_end=committed+uncertain;
    transfer->resume_read=read_target;
    transfer->resume_scratch=scratch;
    transfer->processed=committed;
    if(transfer->source_check_bytes<transfer->resume_check_end)
        transfer->source_check_bytes=transfer->resume_check_end;
    return 1;
}

DollyRestoreTransferState dolly_restore_transfer_step(DollyRestoreTransfer *transfer,
                                                     void *buffer,size_t buffer_bytes) {
    DollyBlockReadResult result;
    uint64_t remaining,offset;
    size_t bytes;
    int checking,verify;
    if(transfer->state!=DOLLY_RESTORE_TRANSFERRING)return transfer->state;
    checking=transfer->source_checked<transfer->source_check_bytes;
    offset=checking?transfer->source_checked:transfer->processed;
    remaining=(checking?transfer->source_check_bytes:transfer->bytes)-offset;
    if(transfer->ranges) {
        const DollyTransferRange *range=&transfer->ranges[transfer->range_index];
        offset=range->offset+transfer->range_processed;
        remaining=range->length-transfer->range_processed;
    }
    transfer->batch_offset=offset;
    bytes=remaining<transfer->batch_bytes?(size_t)remaining:transfer->batch_bytes;
    if(!buffer || buffer_bytes<bytes) {
        transfer->read_status=DOLLY_BLOCK_EINVAL;
        return transfer->state=DOLLY_RESTORE_READ_FAILED;
    }
    result=dolly_block_read(transfer->source,offset,buffer,bytes);
    if(result.status!=DOLLY_BLOCK_OK || result.bytes_read!=bytes) {
        transfer->read_status=result.status;
        return transfer->state=DOLLY_RESTORE_READ_FAILED;
    }
    if(checking) {
        if(offset<transfer->resume_check_end) {
            uint64_t end=offset<transfer->resume_committed?transfer->resume_committed:
                                                         transfer->resume_check_end;
            size_t count=end-offset<bytes?(size_t)(end-offset):bytes;
            DollyBlockReadResult target=transfer->resume_read(transfer->target,offset,
                                                       transfer->resume_scratch,count);
            if(target.status!=DOLLY_BLOCK_OK || target.bytes_read!=count ||
               (offset<transfer->resume_committed &&
                memcmp(buffer,transfer->resume_scratch,count))) {
                transfer->resume_target_failed=1;
                transfer->read_status=DOLLY_BLOCK_EIO;
                return transfer->state=DOLLY_RESTORE_READ_FAILED;
            }
        }
        transfer->source_checked+=bytes;
        return transfer->state;
    }
    verify=transfer->verify || offset<transfer->resume_check_end;
    transfer->write_status=transfer->write_batch(transfer->target,offset,
                                                buffer,bytes,verify);
    if(transfer->write_status!=(verify?DOLLY_WRITE_VERIFIED:DOLLY_WRITE_COMPLETE))
        return transfer->state=DOLLY_RESTORE_WRITE_FAILED;
    if(transfer->ranges) {
        transfer->range_processed+=bytes;
        if(transfer->range_processed==transfer->ranges[transfer->range_index].length) {
            ++transfer->range_index;
            transfer->range_processed=0;
        }
    }
    transfer->processed+=bytes;
    if(transfer->processed==transfer->bytes)transfer->state=DOLLY_RESTORE_FINISHED;
    return transfer->state;
}
