#include "dolly_block_target.h"
#include <string.h>

/* Legacy command offsets are limited to the first 4 GiB. */
#define WRITE_END_LIMIT UINT64_C(0x100000000)

DollyBlockWriteStatus dolly_block_write(const DollyBlockTarget *target,
                      uint64_t offset,const void *data,size_t bytes,
                      int verify,void *scratch,size_t scratch_bytes) {
    uint64_t absolute;
    uintptr_t input=(uintptr_t)data,output=(uintptr_t)scratch;
    size_t written=0;
    DollyBlockReadResult readback;
    if(!target || !target->write || !target->flush ||
       !target->sector_bytes || !target->length ||
       target->start>target->capacity ||
       target->length>target->capacity-target->start ||
       (!target->supports_large_offsets &&
        (target->start>=WRITE_END_LIMIT || target->length>WRITE_END_LIMIT-target->start)) ||
       target->start%target->sector_bytes || target->length%target->sector_bytes ||
       offset%target->sector_bytes || bytes%target->sector_bytes ||
       offset>target->length || bytes>target->length-offset ||
       !data || !bytes || bytes>target->max_transfer)
        return DOLLY_WRITE_REJECTED;
    if(verify && (!target->readback || !scratch || bytes>scratch_bytes ||
                  (input<output?output-input:input-output)<bytes))
        return DOLLY_WRITE_REJECTED;
    absolute=target->start+offset;
    if(target->write(target->context,absolute,data,bytes,&written)!=DOLLY_BLOCK_OK ||
       written!=bytes)return DOLLY_WRITE_FAILED;
    if(target->flush(target->context)!=DOLLY_BLOCK_OK)return DOLLY_WRITE_FLUSH_FAILED;
    if(!verify)return DOLLY_WRITE_COMPLETE;
    readback=target->readback(target->context,absolute,scratch,bytes);
    if(readback.status!=DOLLY_BLOCK_OK || readback.bytes_read!=bytes)
        return DOLLY_WRITE_VERIFY_FAILED;
    return memcmp(data,scratch,bytes)==0?DOLLY_WRITE_VERIFIED:DOLLY_WRITE_MISMATCH;
}

DollyBlockWriteStatus dolly_block_write_verified(const DollyBlockTarget *target,
                      uint64_t offset,const void *data,size_t bytes,
                      void *scratch,size_t scratch_bytes) {
    return dolly_block_write(target,offset,data,bytes,1,scratch,scratch_bytes);
}
