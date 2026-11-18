#include "dolly_amiga_write.h"
#include "amiga_mock_exec.h"
#include <assert.h>
#include <string.h>

static unsigned calls;
static UWORD commands[3];
static UWORD fault;
static int skip_read_data;
static uint64_t dma_address=0x10000;
static unsigned char saved[512],bounce[512];

uint64_t dolly_mock_dma_address(void) { return dma_address; }

LONG DoIO(struct IORequest *io) {
    struct IOStdReq *request=(struct IOStdReq *)io;
    assert(calls<3);
    commands[calls++]=request->io_Command;
    assert(request->io_Actual==0 && request->io_Flags==0 && request->io_Error==0);
    if(request->io_Command==CMD_UPDATE) {
        assert(!request->io_Data && !request->io_Length && !request->io_Offset);
        return fault==CMD_UPDATE?-1:0;
    }
    assert(request->io_Offset==1024 && request->io_Length==sizeof(saved));
    assert(request->io_Data==bounce);
    if(request->io_Command==CMD_WRITE) {
        memcpy(saved,bounce,sizeof(saved));
        request->io_Actual=fault==CMD_WRITE?511:512;
    } else {
        assert(request->io_Command==CMD_READ);
        if(!skip_read_data)memcpy(bounce,saved,sizeof(saved));
        if(fault==CMD_READ)bounce[0]^=1;
        request->io_Actual=512;
    }
    return 0;
}

int main(void) {
    struct IOStdReq request;
    unsigned char input[512];
    DollyAmigaWriteTarget target={
        &request,bounce,sizeof(bounce),2048,512,1024,512,512,0x00fffffe,2
    };
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
    return 0;
}
