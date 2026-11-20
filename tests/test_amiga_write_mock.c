#include "dolly_amiga_write.h"
#include "dolly_clone_names.h"
#include "amiga_mock_exec.h"
#include "dolly_io_plan.h"
#include <assert.h>
#include <string.h>

static unsigned calls;
static UWORD commands[3];
static UWORD fault;
static int skip_read_data;
static uint64_t dma_address=0x10000;
static uint64_t expected_offset=1024;
static size_t expected_bytes=512;
static int expected_nsd64;
static unsigned char saved[1024],bounce[1024];

uint64_t dolly_mock_dma_address(void) { return dma_address; }

LONG DoIO(struct IORequest *io) {
    struct IOStdReq *request=(struct IOStdReq *)io;
    assert(calls<3);
    commands[calls++]=request->io_Command;
    assert(request->io_Flags==0 && request->io_Error==0);
    if(request->io_Command==CMD_UPDATE) {
        assert(!request->io_Data && !request->io_Length && !request->io_Offset &&
               !request->io_Actual);
        return fault==CMD_UPDATE?-1:0;
    }
    assert(request->io_Offset==(uint32_t)expected_offset &&
           request->io_Actual==(uint32_t)(expected_offset>>32) &&
           request->io_Length==expected_bytes);
    assert(request->io_Data==bounce);
    if(request->io_Command==(expected_nsd64?DOLLY_NSCMD_TD_WRITE64:CMD_WRITE)) {
        memcpy(saved,bounce,expected_bytes);
        request->io_Actual=expected_bytes-(fault==request->io_Command);
    } else {
        assert(request->io_Command==(expected_nsd64?DOLLY_NSCMD_TD_READ64:CMD_READ));
        if(!skip_read_data)memcpy(bounce,saved,expected_bytes);
        if(fault==CMD_READ)bounce[0]^=1;
        request->io_Actual=expected_bytes;
    }
    return 0;
}

static void test_volume_label_write(void) {
    struct IOStdReq request;
    unsigned char root[512];
    char label[32];
    uint32_t sum=0,seal;
    unsigned i;
    DollyAmigaWriteTarget target={
        &request,bounce,sizeof(bounce),8192,2048,4096,512,512,0x00fffffe,2,0
    };
    memset(saved,0,sizeof(saved));
    saved[3]=2;
    memset(saved+312,0xff,4);
    saved[511]=1;
    memcpy(saved+432,"\4Work",5);
    for(i=0;i<512;i+=4)
        sum+=(uint32_t)saved[i]<<24 | (uint32_t)saved[i+1]<<16 |
             (uint32_t)saved[i+2]<<8 | saved[i+3];
    seal=0u-sum;
    for(i=0;i<4;++i)saved[20+i]=(unsigned char)(seal>>(24u-8u*i));

    expected_offset=3072;
    assert(dolly_amiga_write_target_read(&target,expected_offset,root,sizeof(root)).status==DOLLY_BLOCK_OK);
    assert(calls==1 && commands[0]==CMD_READ);
    assert(dolly_clone_rename(DOLLY_CLONE_VOLUME_NAME,root,sizeof(root),"Work","Copy_of_Work"));
    calls=0;
    assert(dolly_amiga_write_batch(&target,1024,root,sizeof(root),1)==DOLLY_WRITE_VERIFIED);
    assert(calls==3 && commands[0]==CMD_WRITE && commands[1]==CMD_UPDATE && commands[2]==CMD_READ);
    assert(dolly_clone_read_name(DOLLY_CLONE_VOLUME_NAME,saved,512,label));
    assert(!strcmp(label,"Copy_of_Work"));
    calls=0;
    expected_offset=1024;
}

int main(void) {
    struct IOStdReq request;
    unsigned char input[512],crossing[1024];
    DollyAmigaWriteTarget target={
        &request,bounce,sizeof(bounce),2048,512,1024,512,512,0x00fffffe,2,0
    };
    test_volume_label_write();
    memset(input,0xa5,sizeof(input));
    assert(dolly_amiga_write_batch(&target,512,input,sizeof(input),0)==DOLLY_WRITE_COMPLETE);
    assert(calls==2 && commands[0]==CMD_WRITE && commands[1]==CMD_UPDATE);
    assert(memcmp(input,saved,sizeof(input))==0);
    calls=0;
    assert(dolly_amiga_write_batch(&target,512,input,sizeof(input),1)==DOLLY_WRITE_VERIFIED);
    assert(calls==3 && commands[0]==CMD_WRITE && commands[1]==CMD_UPDATE &&
           commands[2]==CMD_READ);
    calls=0;
    assert(dolly_amiga_write_batch(&target,1024,input,sizeof(input),1)==DOLLY_WRITE_REJECTED);
    assert(dolly_amiga_write_batch(&target,512,bounce,sizeof(bounce),0)==DOLLY_WRITE_REJECTED);
    dma_address=0x1000000;
    assert(dolly_amiga_write_batch(&target,512,input,sizeof(input),0)==DOLLY_WRITE_REJECTED);
    assert(calls==0);
    dma_address=0x10000;
    fault=CMD_WRITE;
    assert(dolly_amiga_write_batch(&target,512,input,sizeof(input),1)==DOLLY_WRITE_FAILED);
    assert(calls==1);
    calls=0;
    fault=CMD_UPDATE;
    assert(dolly_amiga_write_batch(&target,512,input,sizeof(input),1)==DOLLY_WRITE_FLUSH_FAILED);
    assert(calls==2);
    calls=0;
    fault=CMD_READ;
    assert(dolly_amiga_write_batch(&target,512,input,sizeof(input),1)==DOLLY_WRITE_MISMATCH);
    assert(calls==3);
    calls=0;
    fault=0;
    skip_read_data=1;
    assert(dolly_amiga_write_batch(&target,512,input,sizeof(input),1)==DOLLY_WRITE_MISMATCH);
    assert(calls==3);
    calls=0;
    skip_read_data=0;
    target.start=UINT64_C(0xfffffe00);
    target.length=2048;
    target.capacity=target.start+target.length;
    target.max_transfer=1024;
    memset(crossing,0xa5,sizeof(crossing));
    expected_offset=target.start;
    expected_bytes=sizeof(crossing);
    assert(dolly_amiga_write_batch(&target,0,crossing,sizeof(crossing),1)==DOLLY_WRITE_REJECTED);
    assert(calls==0);
    target.nsd64=1;
    expected_bytes=sizeof(input);
    assert(dolly_amiga_write_batch(&target,0,input,sizeof(input),1)==DOLLY_WRITE_VERIFIED);
    assert(calls==3 && commands[0]==CMD_WRITE && commands[2]==CMD_READ);
    calls=0;
    expected_bytes=sizeof(crossing);
    expected_nsd64=1;
    assert(dolly_amiga_write_batch(&target,0,crossing,sizeof(crossing),1)==DOLLY_WRITE_VERIFIED);
    assert(calls==3 && commands[0]==DOLLY_NSCMD_TD_WRITE64 &&
           commands[1]==CMD_UPDATE && commands[2]==DOLLY_NSCMD_TD_READ64);
    calls=0;
    expected_offset=UINT64_C(0x100000200);
    expected_bytes=sizeof(input);
    assert(dolly_amiga_write_batch(&target,1024,input,sizeof(input),1)==DOLLY_WRITE_VERIFIED);
    assert(calls==3);
    calls=0;
    fault=DOLLY_NSCMD_TD_WRITE64;
    assert(dolly_amiga_write_batch(&target,1024,input,sizeof(input),1)==DOLLY_WRITE_FAILED);
    assert(calls==1 && commands[0]==DOLLY_NSCMD_TD_WRITE64);
    return 0;
}
