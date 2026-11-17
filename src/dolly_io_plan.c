#include "dolly_io_plan.h"
#include <stdint.h>
#include <string.h>
DollyBlockStatus dolly_io_plan_read(uint64_t offset, size_t requested,
                                   uint32_t block_bytes, uint32_t max_transfer,
                                   int nsd64, DollyIoReadPlan *out){
    uint64_t n;
    if(!out || block_bytes==0 || max_transfer<block_bytes ||
       max_transfer%block_bytes || !requested ||
       offset%block_bytes || requested%block_bytes)
        return DOLLY_BLOCK_EINVAL;
    memset(out,0,sizeof(*out));
    n=requested;
    if(n>max_transfer)n=max_transfer;
    if(offset>UINT64_MAX-n)return DOLLY_BLOCK_ERANGE;
    if(!nsd64 && offset+n>UINT64_C(0x100000000))
        return DOLLY_BLOCK_EUNSUPPORTED;
    out->command=nsd64 && offset+n>UINT64_C(0x100000000)
                 ? DOLLY_NSCMD_TD_READ64 : DOLLY_CMD_READ;
    out->offset_low=(uint32_t)offset;
    out->offset_high=out->command==DOLLY_NSCMD_TD_READ64
                      ? (uint32_t)(offset>>32) : 0;
    out->byte_count=(uint32_t)n;
    return DOLLY_BLOCK_OK;
}
