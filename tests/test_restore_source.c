#include "dolly_restore_source.h"
#include "dolly_restore_check.h"
#include "dolly_restore_transfer.h"
#include <assert.h>
#include <string.h>

static DollyBlockReadResult read_memory(void *context,uint64_t at,void *data,size_t bytes) {
    memcpy(data,(unsigned char *)context+at,bytes);
    return (DollyBlockReadResult){DOLLY_BLOCK_OK,bytes};
}
static void close_memory(void *context) { (void)context; }

static void make_rdb(unsigned char *data,unsigned cylinders) {
    unsigned values[19]={0x5244534b,19,0,0,512,0,0,0xffffffffu,
                         0,0,0,0,0,0,0,0,cylinders,16,4};
    unsigned i;
    for(i=0;i<19;++i)values[2]-=i==2?0:values[i];
    memset(data,0,512);
    for(i=0;i<19;++i) {
        data[i*4]=(unsigned char)(values[i]>>24);
        data[i*4+1]=(unsigned char)(values[i]>>16);
        data[i*4+2]=(unsigned char)(values[i]>>8);
        data[i*4+3]=(unsigned char)values[i];
    }
}

static void check_assessment(void) {
    unsigned char a[1024]={'D','O','S',1},b[1536]={0};
    DollyRestoreExtent source={NULL,0,sizeof(a),512,DOLLY_RESTORE_UNKNOWN};
    DollyRestoreExtent target={NULL,512,sizeof(a),512,DOLLY_RESTORE_PARTITION};
    DollyRestoreAssessment result;
    memcpy(b+512,a,sizeof(a));
    assert(dolly_block_open_backend(sizeof(a),read_memory,close_memory,a,&source.blocks)==DOLLY_BLOCK_OK);
    assert(dolly_block_open_backend(sizeof(b),read_memory,close_memory,b,&target.blocks)==DOLLY_BLOCK_OK);
    assert(dolly_restore_assess(&source,&target).decision==DOLLY_RESTORE_MATCH);
    b[515]=3;
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"Filesystem ID"));
    b[512]='?';
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"Target:"));
    make_rdb(a,100);
    memcpy(b+512,a,sizeof(a));
    target.expected_kind=DOLLY_RESTORE_RDB_DISK;
    assert(dolly_restore_assess(&source,&target).decision==DOLLY_RESTORE_MATCH);
    make_rdb(b+512,101);
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"RDB geometry"));
    memset(b+512,0,512);
    memcpy(b+512,"DOS\1",4);
    target.expected_kind=DOLLY_RESTORE_PARTITION;
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"whole RDB disk"));
    memset(a,0,sizeof(a));
    a[510]=0x55;
    a[511]=0xaa;
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_BLOCKED && strstr(result.reason,"MBR"));
    memset(a,0,sizeof(a));
    memcpy(a,"FORM",4);
    memcpy(a+8,"DOLY",4);
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_BLOCKED && strstr(result.reason,"IFF DOLY"));
    source.bytes=512;
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_BLOCKED && result.size==DOLLY_RESTORE_SIZE_SMALLER);
    dolly_block_close(source.blocks);
    dolly_block_close(target.blocks);
}

typedef struct TransferFixture {
    unsigned char dest[1536];
    unsigned calls;
    unsigned fail_at;
} TransferFixture;

static DollyBlockWriteStatus write_test_batch(void *context,uint64_t offset,
                                       const void *data,size_t bytes,int verify) {
    TransferFixture *f=context;
    assert(verify && offset+bytes<=sizeof(f->dest));
    ++f->calls;
    if(f->calls==f->fail_at)return DOLLY_WRITE_FAILED;
    memcpy(f->dest+offset,data,bytes);
    return DOLLY_WRITE_VERIFIED;
}

static void check_transfer(void) {
    unsigned char data[1536],buffer[1024];
    DollyRestoreTransfer transfer;
    DollyBlockSource *source;
    TransferFixture fixture={0};
    memset(data,0x6a,sizeof(data));
    assert(dolly_block_open_backend(sizeof(data),read_memory,close_memory,data,&source)==DOLLY_BLOCK_OK);
    assert(!dolly_restore_transfer_init(&transfer,source,1024,512,1024,
                                        &fixture,write_test_batch,1));
    assert(dolly_restore_transfer_init(&transfer,source,sizeof(data),512,1024,
                                       &fixture,write_test_batch,1));
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_TRANSFERRING);
    assert(transfer.processed==1024 && fixture.calls==1);
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_FINISHED);
    assert(transfer.processed==sizeof(data) && fixture.calls==2);
    assert(memcmp(data,fixture.dest,sizeof(data))==0);

    fixture.calls=0;
    fixture.fail_at=2;
    assert(dolly_restore_transfer_init(&transfer,source,sizeof(data),512,1024,
                                       &fixture,write_test_batch,1));
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_TRANSFERRING);
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_WRITE_FAILED);
    assert(transfer.processed==1024 && fixture.calls==2);
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_WRITE_FAILED);
    assert(fixture.calls==2);
    assert(dolly_restore_transfer_init(&transfer,source,sizeof(data),512,1024,
                                       &fixture,write_test_batch,1));
    assert(dolly_restore_transfer_step(&transfer,buffer,512)==DOLLY_RESTORE_READ_FAILED);
    assert(fixture.calls==2);
    dolly_block_close(source);
}

int main(void) {
    assert(dolly_restore_check_raw(4096,4096,512)==DOLLY_BLOCK_OK);
    assert(dolly_restore_check_raw(4096,4096,4096)==DOLLY_BLOCK_OK);
    assert(dolly_restore_check_raw(4096,8192,512)==DOLLY_BLOCK_ERANGE);
    assert(dolly_restore_check_raw(8192,4096,512)==DOLLY_BLOCK_ERANGE);
    assert(dolly_restore_check_raw(4095,4096,512)==DOLLY_BLOCK_EINVAL);
    assert(dolly_restore_check_raw(4096,4095,512)==DOLLY_BLOCK_EINVAL);
    assert(dolly_restore_check_raw(0,0,512)==DOLLY_BLOCK_EINVAL);
    assert(dolly_restore_check_raw(6144,6144,3072)==DOLLY_BLOCK_EINVAL);
    assert(dolly_restore_check_raw(4096,4096,0)==DOLLY_BLOCK_EINVAL);
    check_assessment();
    check_transfer();
    return 0;
}
