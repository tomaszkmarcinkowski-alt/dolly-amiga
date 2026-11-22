#include "dolly_restore_source.h"
#include "dolly_restore_check.h"
#include "dolly_restore_transfer.h"
#include "dolly_apple_media.h"
#include <assert.h>
#include <string.h>

static DollyBlockReadResult read_memory(void *context,uint64_t at,void *data,size_t bytes) {
    memcpy(data,(unsigned char *)context+at,bytes);
    return (DollyBlockReadResult){DOLLY_BLOCK_OK,bytes};
}
static void close_memory(void *context) { (void)context; }

static void put16(unsigned char *at,unsigned value) {
    at[0]=(unsigned char)(value>>8);
    at[1]=(unsigned char)value;
}

static void put32(unsigned char *at,uint32_t value) {
    put16(at,(unsigned)(value>>16));
    put16(at+2,(unsigned)value);
}

static void plus_header(unsigned char *header,unsigned signature,unsigned version,unsigned blocks) {
    put16(header,signature);
    put16(header+2,version);
    put32(header+40,512);
    put32(header+44,blocks);
}

static void map_entry(unsigned char *entry,const char *type,unsigned first,unsigned count) {
    memcpy(entry,"PM",2);
    put32(entry+4,2);
    put32(entry+8,first);
    put32(entry+12,count);
    strcpy((char *)entry+48,type);
    put32(entry+84,count);
}

static DollyBlockReadResult read_sparse_apple(void *context,uint64_t at,void *data,size_t bytes) {
    const size_t prefix=8u*512u;
    memset(data,0,bytes);
    if(at<prefix) {
        size_t available=prefix-(size_t)at;
        memcpy(data,(unsigned char *)context+(size_t)at,bytes<available?bytes:available);
    }
    return (DollyBlockReadResult){DOLLY_BLOCK_OK,bytes};
}

static void check_shapeshifter_limits(void) {
    const uint64_t two_gib=UINT64_C(1)<<31,four_gib=UINT64_C(1)<<32;
    const uint32_t sectors_2gib=UINT32_C(1)<<22;
    unsigned char data[8u*512u]={0};
    unsigned char *entry=data+2u*512u;
    DollyBlockSource *source=NULL;
    int oversized;
    assert(dolly_shapeshifter_file_fits(two_gib-512u));
    assert(!dolly_shapeshifter_file_fits(two_gib));
    assert(!dolly_shapeshifter_file_fits(two_gib-1u));
    assert(dolly_shapeshifter_partition_fits(four_gib-2u*512u,2u*512u));
    assert(!dolly_shapeshifter_partition_fits(four_gib-2u*512u,3u*512u));
    assert(!dolly_shapeshifter_partition_fits(four_gib,512u));
    assert(!dolly_shapeshifter_partition_fits(UINT64_MAX-511u,512u));
    assert(dolly_block_open_backend(four_gib,read_sparse_apple,close_memory,data,&source)==DOLLY_BLOCK_OK);

    plus_header(entry,0x482b,4,sectors_2gib-1u);
    assert(dolly_shapeshifter_media_limits(source,0,two_gib-512u,&oversized)==DOLLY_APPLE_HFS_PLUS && !oversized);
    assert(dolly_shapeshifter_media_limits(source,0,two_gib,&oversized)==DOLLY_APPLE_HFS_PLUS && oversized);
    memset(data,0,sizeof(data));
    map_entry(data+512,"Apple_partition_map",1,3);
    map_entry(entry,"Apple_HFS",4,sectors_2gib-1u);
    /* A small logical HFS data area does not hide an oversized APM partition. */
    put32(entry+84,12);
    plus_header(data+6u*512u,0x482b,4,12);
    assert(dolly_shapeshifter_media_limits(source,0,four_gib,&oversized)==DOLLY_APPLE_APM && !oversized);
    put32(entry+12,sectors_2gib);
    assert(dolly_shapeshifter_media_limits(source,0,four_gib,&oversized)==DOLLY_APPLE_APM && oversized);
    memset(entry+48,0,32);
    strcpy((char *)entry+48,"Apple_Free");
    assert(dolly_shapeshifter_media_limits(source,0,four_gib,&oversized)==DOLLY_APPLE_APM && !oversized);
    dolly_block_close(source);
}

static void check_apple_media(void) {
    unsigned char image[16u*512u]={0},target[17u*512u]={0};
    unsigned char *header=image+1024;
    unsigned char *disk=target+512;
    DollyRestoreExtent source={NULL,0,sizeof(image),512,DOLLY_RESTORE_UNKNOWN};
    DollyRestoreExtent destination={NULL,512,sizeof(image),512,DOLLY_RESTORE_PARTITION};
    assert(dolly_block_open_backend(sizeof(image),read_memory,close_memory,image,&source.blocks)==DOLLY_BLOCK_OK);
    assert(dolly_block_open_backend(sizeof(target),read_memory,close_memory,target,&destination.blocks)==DOLLY_BLOCK_OK);
    assert(dolly_apple_media_probe(source.blocks,0,sizeof(image))==DOLLY_APPLE_UNKNOWN);
    plus_header(header,0x482b,4,16);
    assert(dolly_apple_media_probe(source.blocks,0,sizeof(image))==DOLLY_APPLE_HFS_PLUS);
    assert(dolly_apple_media_probe(source.blocks,0,sizeof(image)-512)==DOLLY_APPLE_INVALID);
    plus_header(header,0x4858,5,16);
    assert(dolly_apple_media_probe(source.blocks,0,sizeof(image))==DOLLY_APPLE_HFSX);
    memset(header,0,512);
    memcpy(header,"BD",2);
    put16(header+14,3);
    put16(header+18,10);
    put32(header+20,512);
    put16(header+28,4);
    assert(dolly_apple_media_probe(source.blocks,0,sizeof(image))==DOLLY_APPLE_HFS);
    memcpy(header+124,"H+",2);
    put16(header+126,1);
    put16(header+128,8);
    plus_header(image+7u*512u,0x482b,4,8);
    assert(dolly_apple_media_probe(source.blocks,0,sizeof(image))==DOLLY_APPLE_WRAPPED_HFS_PLUS);
    put16(header+128,10);
    assert(dolly_apple_media_probe(source.blocks,0,sizeof(image))==DOLLY_APPLE_INVALID);
    put16(header+128,8);

    memcpy(disk,"ER",2);
    put16(disk+2,512);
    put32(disk+4,16);
    map_entry(disk+512,"Apple_partition_map",1,3);
    map_entry(disk+1024,"Apple_HFS",4,12);
    plus_header(disk+6u*512u,0x482b,4,12);
    DollyAppleMap map;
    assert(dolly_apple_media_inspect(destination.blocks,512,sizeof(image),&map)==DOLLY_APPLE_APM);
    assert(map.count==1 && map.volumes[0].entry==2 && map.volumes[0].offset==4u*512u &&
           map.volumes[0].bytes==12u*512u && map.volumes[0].media==DOLLY_APPLE_HFS_PLUS);
    DollyRestoreAssessment result=dolly_restore_assess(&source,&destination);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"HFS wrapper") &&
           strstr(result.reason,"Apple Partition Map disk"));
    put32(disk+1024+8,3);
    assert(dolly_apple_media_inspect(destination.blocks,512,sizeof(image),&map)==DOLLY_APPLE_INVALID);
    assert(map.count==0);
    put32(disk+1024+8,4);
    put32(disk+1024+80,1);
    assert(dolly_apple_media_probe(destination.blocks,512,sizeof(image))==DOLLY_APPLE_INVALID);
    put16(disk+2,2048);
    assert(dolly_apple_media_probe(destination.blocks,512,sizeof(image))==DOLLY_APPLE_UNSUPPORTED);
    dolly_block_close(destination.blocks);
    dolly_block_close(source.blocks);
}

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

/* A checksummed PART descriptor whose declared extent overruns the image. */
static void add_overrunning_partition(unsigned char data[1024]) {
    unsigned char *part=data+512;
    const unsigned words[49]={[0]=0x50415254,[1]=49,[4]=0xffffffffu,
        [32]=16,[33]=128,[35]=1,[36]=1,[37]=1,[41]=1,[42]=2,
        [48]=0x444f5301};
    unsigned i,j,sum=0;
    data[31]=1;
    /* Replace END with block 1 and adjust the RDSK checksum by -2. */
    memset(data+28,0,3);
    data[11]-=2;
    for(i=0;i<49;++i)sum+=words[i];
    for(i=0;i<49;++i) {
        unsigned word=i==2?0u-sum:words[i];
        for(j=0;j<4;++j)part[i*4+j]=(unsigned char)(word>>(24-j*8));
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
    make_rdb(a,100);
    add_overrunning_partition(a);
    memcpy(b+512,a,sizeof(a));
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"Image and destination RDB"));
    make_rdb(b+512,100);
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"Image RDB extends beyond capacity"));
    memset(b+512,0,512);
    a[512+11]^=1; /* Corrupt the PART checksum with an equal-sized blank target. */
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"Checksum mismatch"));
    make_rdb(a,100);
    memset(b+512,0,512);
    memcpy(b+512,"DOS\1",4);
    target.expected_kind=DOLLY_RESTORE_PARTITION;
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"whole RDB disk"));
    memset(a,0,sizeof(a));
    a[510]=0x55;
    a[511]=0xaa;
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MISMATCH && strstr(result.reason,"MBR"));
    memset(a,0,sizeof(a));
    memcpy(a,"FORM",4);
    memcpy(a+8,"DOLY",4);
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_BLOCKED && strstr(result.reason,"IFF DOLY"));
    source.bytes=512;
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_BLOCKED && result.size==DOLLY_RESTORE_SIZE_SMALLER);
    memcpy(a,"DOS\1",4);
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MATCH && result.size==DOLLY_RESTORE_SIZE_SMALLER);
    source.bytes=1024;
    target.bytes=512;
    result=dolly_restore_assess(&source,&target);
    assert(result.decision==DOLLY_RESTORE_MATCH && result.size==DOLLY_RESTORE_SIZE_LARGER);
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

static DollyBlockReadResult read_resume_test(void *context,uint64_t at,void *data,size_t bytes) {
    TransferFixture *f=context;
    if(f->fail_at==at+1)return (DollyBlockReadResult){DOLLY_BLOCK_EIO,0};
    return read_memory(f->dest,at,data,bytes);
}

static DollyBlockWriteStatus write_resume_test(void *context,uint64_t offset,
                                      const void *data,size_t bytes,int verify) {
    TransferFixture *f=context;
    assert(offset==(f->calls?1024u:512u));
    assert(verify==!f->calls && bytes==512);
    ++f->calls;
    memcpy(f->dest+offset,data,bytes);
    return verify?DOLLY_WRITE_VERIFIED:DOLLY_WRITE_COMPLETE;
}

static void check_resume(void) {
    unsigned char data[1536],buffer[512],scratch[512];
    DollyRestoreTransfer transfer;
    DollyBlockSource *source;
    TransferFixture fixture={0};
    memset(data,0x6a,sizeof(data));
    memcpy(fixture.dest,data,512+137); /* the failed batch wrote 137 bytes */
    assert(dolly_block_open_backend(sizeof(data),read_memory,close_memory,data,&source)==DOLLY_BLOCK_OK);
    assert(dolly_restore_transfer_init(&transfer,source,sizeof(data),512,512,
                                       &fixture,write_resume_test,0));
    assert(!dolly_restore_transfer_set_resume(&transfer,13,512,512,read_resume_test,
                                               scratch,sizeof(scratch)));
    assert(dolly_restore_transfer_set_resume(&transfer,512,512,512,read_resume_test,
                                              scratch,sizeof(scratch)));
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_TRANSFERRING);
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_TRANSFERRING);
    assert(fixture.calls==0 && transfer.processed==512 && transfer.source_checked==1024);
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_TRANSFERRING);
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_FINISHED);
    assert(fixture.calls==2 && !memcmp(fixture.dest,data,sizeof(data)));

    for(unsigned changed=0;changed<2;++changed) {
        fixture.calls=0;
        fixture.fail_at=changed?0:513;
        if(changed)fixture.dest[10]^=1;
        assert(dolly_restore_transfer_init(&transfer,source,sizeof(data),512,512,
                                           &fixture,write_resume_test,0));
        assert(dolly_restore_transfer_set_resume(&transfer,512,512,512,read_resume_test,
                                                  scratch,sizeof(scratch)));
        if(!changed)
            assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_TRANSFERRING);
        assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_READ_FAILED);
        assert(fixture.calls==0 && transfer.resume_target_failed);
    }
    dolly_block_close(source);
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

    {
        const DollyTransferRange ranges[]={{0,512},{1024,512}};
        const DollyTransferRange invalid[]={{0,1024},{512,512}};
        fixture=(TransferFixture){0};
        memset(fixture.dest,0x21,sizeof(fixture.dest));
        assert(dolly_restore_transfer_init(&transfer,source,sizeof(data),512,1024,
                                           &fixture,write_test_batch,1));
        assert(!dolly_restore_transfer_set_ranges(&transfer,invalid,2,512));
        assert(dolly_restore_transfer_set_ranges(&transfer,ranges,2,512));
        assert(transfer.bytes==1024);
        assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_TRANSFERRING);
        assert(transfer.batch_offset==0 && transfer.processed==512);
        assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_FINISHED);
        assert(transfer.batch_offset==1024 && transfer.processed==1024 && fixture.calls==2);
        assert(!memcmp(fixture.dest,data,512) && !memcmp(fixture.dest+1024,data+1024,512));
        for(size_t i=512;i<1024;++i)assert(fixture.dest[i]==0x21);
    }

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
    fixture=(TransferFixture){0};
    assert(dolly_restore_transfer_init_prefix(&transfer,source,1024,512,1024,
                                               &fixture,write_test_batch,1));
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_FINISHED);
    assert(transfer.processed==1024 && fixture.calls==1 && fixture.dest[1024]==0);
    dolly_block_close(source);

    fixture=(TransferFixture){0};
    assert(dolly_block_open_backend(512,read_memory,close_memory,data,&source)==DOLLY_BLOCK_OK);
    assert(dolly_restore_transfer_init_prefix(&transfer,source,sizeof(fixture.dest),512,1024,
                                               &fixture,write_test_batch,1));
    assert(dolly_restore_transfer_step(&transfer,buffer,sizeof(buffer))==DOLLY_RESTORE_FINISHED);
    assert(transfer.processed==512 && fixture.calls==1 && fixture.dest[512]==0);
    dolly_block_close(source);
}

int main(void) {
    check_resume();
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
    check_apple_media();
    check_shapeshifter_limits();
    check_transfer();
    return 0;
}
