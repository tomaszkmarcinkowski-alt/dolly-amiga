#include "dolly_io_plan.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
    DollyIoReadPlan p;
    assert(dolly_io_plan_read(0,1024,512,4096,0,&p)==DOLLY_BLOCK_OK);
    assert(p.command==DOLLY_CMD_READ && p.byte_count==1024 && p.offset_low==0);
    assert(dolly_io_plan_read(512,8192,512,4096,0,&p)==DOLLY_BLOCK_OK);
    assert(p.byte_count==4096 && p.offset_low==512);
    assert(dolly_io_plan_read(UINT64_C(0xffffff00),512,512,4096,0,&p)==DOLLY_BLOCK_EINVAL);
    assert(dolly_io_plan_read(UINT64_C(0xfffffe00),512,512,4096,0,&p)==DOLLY_BLOCK_OK);
    assert(p.command==DOLLY_CMD_READ && p.offset_low==0xfffffe00u);
    assert(dolly_io_plan_read(UINT64_C(0xfffffe00),1024,512,4096,0,&p)==DOLLY_BLOCK_EUNSUPPORTED);
    assert(dolly_io_plan_read(UINT64_C(0xfffffe00),1024,512,4096,1,&p)==DOLLY_BLOCK_OK);
    assert(p.command==DOLLY_NSCMD_TD_READ64 && p.offset_high==0);
    assert(dolly_io_plan_read(UINT64_C(0x100000000),512,512,4096,0,&p)==DOLLY_BLOCK_EUNSUPPORTED);
    assert(dolly_io_plan_read(UINT64_C(0x100000000),512,512,4096,1,&p)==DOLLY_BLOCK_OK);
    assert(p.command==DOLLY_NSCMD_TD_READ64 && p.offset_high==1 && p.offset_low==0);
    assert(dolly_io_plan_read(UINT64_C(0x100000200),512,512,4096,1,&p)==DOLLY_BLOCK_OK);
    assert(p.offset_high==1 && p.offset_low==512);
    assert(dolly_io_plan_read(UINT64_MAX,512,512,4096,1,&p)==DOLLY_BLOCK_EINVAL);
    assert(dolly_io_plan_read(UINT64_MAX-511,512,512,4096,1,&p)==DOLLY_BLOCK_ERANGE);
    assert(dolly_io_plan_read(0,0,512,4096,0,&p)==DOLLY_BLOCK_EINVAL);
    assert(dolly_io_plan_read(0,513,512,4096,0,&p)==DOLLY_BLOCK_EINVAL);
    assert(dolly_io_plan_read(0,512,512,700,0,&p)==DOLLY_BLOCK_EINVAL);
    puts("read planner tests passed");return 0;
}
