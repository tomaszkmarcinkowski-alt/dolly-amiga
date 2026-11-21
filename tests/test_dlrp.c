#include "dolly_dlrp.h"
#include "dolly_checksum.h"
#include "dolly_restore_transfer.h"
#include <assert.h>
#include <string.h>

typedef struct Buffer {
    unsigned char bytes[512];
    size_t length,position;
} Buffer;

static int write_bytes(void *ctx,const void *data,size_t size) {
    Buffer *out=ctx;
    if(size>sizeof(out->bytes)-out->length)return 0;
    memcpy(out->bytes+out->length,data,size);
    out->length+=size;
    return 1;
}

static int read_bytes(void *ctx,void *data,size_t size) {
    Buffer *in=ctx;
    if(size>in->length-in->position)return 0;
    memcpy(data,in->bytes+in->position,size);
    in->position+=size;
    return 1;
}

static void checksum(Buffer *file) {
    DollyChecksum crc;
    dolly_checksum_init(&crc);
    assert(dolly_checksum_update(&crc,file->bytes,file->length-12)==0);
    dolly_dlrp_u32(file->bytes+file->length-4,dolly_checksum_final(&crc));
}

static Buffer image_checkpoint(DollyImageFormat format) {
    Buffer file={0};
    unsigned char head[108]={0},geometry[36]={0},output[24]={0},crc[4]={0};
    dolly_dlrp_u32(head,1);
    dolly_dlrp_u32(head+4,2);
    dolly_dlrp_u32(head+8,DOLLY_ENDPOINT_UNIT);
    dolly_dlrp_u32(head+16,512);
    dolly_dlrp_u32(head+20,16384);
    dolly_dlrp_u64(head+32,64);
    dolly_dlrp_u64(head+40,32);
    dolly_dlrp_u64(head+48,64);
    dolly_dlrp_u32(head+80,1);
    dolly_dlrp_u32(head+84,0x31415926);
    dolly_dlrp_u32(output,format);
    dolly_dlrp_u32(output+4,format==DOLLY_IMAGE_IFF_FAST?196:16384);
    if(format==DOLLY_IMAGE_IFF_FAST) {
        dolly_dlrp_u32(output+8,DOLLY_DLY_PACKBITS);
        dolly_dlrp_u32(output+12,32884);
        dolly_dlrp_u64(output+16,32);
    }
    /* Finalize the FORM length before calculating the checksum. */
    assert(dolly_dlrp_form(write_bytes,&file,0));
    assert(dolly_dlrp_chunk(write_bytes,&file,"HEAD",head,sizeof(head)));
    assert(dolly_dlrp_chunk(write_bytes,&file,"GEOM",geometry,sizeof(geometry)));
    assert(dolly_dlrp_chunk(write_bytes,&file,"SRCE","scsi.device / Unit 0",20));
    assert(dolly_dlrp_chunk(write_bytes,&file,"DRVR","scsi.device",11));
    assert(dolly_dlrp_chunk(write_bytes,&file,"NAME","Drive",5));
    assert(dolly_dlrp_chunk(write_bytes,&file,"OUTP",output,sizeof(output)));
    assert(dolly_dlrp_chunk(write_bytes,&file,"DEST","RAM:test.hdf",12));
    assert(dolly_dlrp_chunk(write_bytes,&file,"PART","RAM:test.hdf.part/image",23));
    assert(dolly_dlrp_chunk(write_bytes,&file,"CR32",crc,sizeof(crc)));
    dolly_dlrp_u32(file.bytes+4,(uint32_t)file.length-8);
    checksum(&file);
    return file;
}

static DollyDlrpStatus decode(Buffer file,DollyDlrpImageResume *out) {
    return dolly_dlrp_read_image(read_bytes,&file,(uint32_t)file.length,out);
}

static void test_image_reader(void) {
    DollyDlrpImageResume r;
    Buffer valid=image_checkpoint(DOLLY_IMAGE_HDF),bad=valid;
    assert(decode(valid,&r)==DOLLY_DLRP_READY);
    assert(r.source.kind==DOLLY_ENDPOINT_UNIT && r.target.image_format==DOLLY_IMAGE_HDF);
    assert(r.completed==32 && r.sectors==64 && r.output_bytes==16384 && r.first_crc==0x31415926);
    assert(!strcmp(r.partial,"RAM:test.hdf.part/image"));
    --bad.length;
    assert(decode(bad,&r)==DOLLY_DLRP_INVALID);
    bad=valid;
    bad.bytes[bad.length-1]^=1;
    assert(decode(bad,&r)==DOLLY_DLRP_INVALID);
    bad=valid;
    dolly_dlrp_u64(bad.bytes+20+40,65);
    checksum(&bad);
    assert(decode(bad,&r)==DOLLY_DLRP_INVALID);
    bad=valid;
    dolly_dlrp_u32(bad.bytes+20+80,0);
    checksum(&bad);
    assert(decode(bad,&r)==DOLLY_DLRP_INVALID);
    bad=valid;
    dolly_dlrp_u32(bad.bytes+20,2);
    checksum(&bad);
    assert(decode(bad,&r)==DOLLY_DLRP_UNSUPPORTED);
    valid=image_checkpoint(DOLLY_IMAGE_IFF_FAST);
    assert(decode(valid,&r)==DOLLY_DLRP_READY);
    assert(r.codec==DOLLY_DLY_PACKBITS && r.batch_bytes==16384 &&
           r.dly_file_bytes==32884 && r.output_bytes==196 && r.completed==32);
    dolly_dlrp_u64(valid.bytes+20+40,31);
    checksum(&valid);
    assert(decode(valid,&r)==DOLLY_DLRP_INVALID);
}

static Buffer transfer_checkpoint(int restore) {
    Buffer file={0};
    unsigned char head[56]={0},source[76]={0},target[76]={0},crc[4]={0};
    const char *path=restore?"RAM:test.hdf":"scsi.device / Unit 0";
    dolly_dlrp_u32(head,2);
    dolly_dlrp_u32(head+4,restore?4:3);
    dolly_dlrp_u32(head+8,512);
    dolly_dlrp_u32(head+12,512);
    dolly_dlrp_u32(head+20,DOLLY_RESTORE_WRITE_FAILED);
    dolly_dlrp_u64(head+24,512);
    dolly_dlrp_u64(head+32,512);
    dolly_dlrp_u64(head+40,2048);
    dolly_dlrp_u32(head+52,DOLLY_WRITE_FAILED);
    dolly_dlrp_u32(source,restore?DOLLY_ENDPOINT_FILE:DOLLY_ENDPOINT_UNIT);
    dolly_dlrp_u32(source+48,restore?DOLLY_IMAGE_HDF:0);
    dolly_dlrp_u64(source+24,2048);
    dolly_dlrp_u64(source+32,2048);
    dolly_dlrp_u32(target,DOLLY_ENDPOINT_PARTITION);
    dolly_dlrp_u32(target+4,1);
    dolly_dlrp_u64(target+16,2048);
    dolly_dlrp_u64(target+24,2048);
    dolly_dlrp_u64(target+32,4096);
    assert(dolly_dlrp_form(write_bytes,&file,0));
    assert(dolly_dlrp_chunk(write_bytes,&file,"HEAD",head,sizeof(head)));
    assert(dolly_dlrp_chunk(write_bytes,&file,"SRC2",source,sizeof(source)));
    assert(dolly_dlrp_chunk(write_bytes,&file,"DST2",target,sizeof(target)));
    assert(dolly_dlrp_chunk(write_bytes,&file,"SRCE",path,strlen(path)));
    assert(dolly_dlrp_chunk(write_bytes,&file,"DEST","scsi.device / Unit 1 / DH1",26));
    assert(dolly_dlrp_chunk(write_bytes,&file,"DRVR","scsi.device",restore?0:11));
    assert(dolly_dlrp_chunk(write_bytes,&file,"TDRV","scsi.device",11));
    assert(dolly_dlrp_chunk(write_bytes,&file,"NAME","Source",restore?0:6));
    assert(dolly_dlrp_chunk(write_bytes,&file,"TNAM","Target",6));
    assert(dolly_dlrp_chunk(write_bytes,&file,"CR32",crc,sizeof(crc)));
    dolly_dlrp_u32(file.bytes+4,(uint32_t)file.length-8);
    checksum(&file);
    return file;
}

static DollyDlrpStatus decode_transfer(Buffer file,DollyDlrpTransferResume *out) {
    return dolly_dlrp_read_transfer(read_bytes,&file,(uint32_t)file.length,out);
}

static void test_transfer_reader(void) {
    for(int restore=0;restore<=1;++restore) {
        DollyDlrpTransferResume r;
        Buffer good=transfer_checkpoint(restore),bad=good;
        assert(decode_transfer(good,&r)==DOLLY_DLRP_READY);
        assert(r.restoring==restore && r.committed==512 && r.uncertain==512 && r.bytes==2048);
        assert(r.target.unit==1 && r.target.endpoint.partition_start_bytes==2048);
        bad.bytes[bad.length-1]^=1;
        assert(decode_transfer(bad,&r)==DOLLY_DLRP_INVALID);
        bad=good;
        dolly_dlrp_u64(bad.bytes+20+32,0);
        checksum(&bad);
        assert(decode_transfer(bad,&r)==DOLLY_DLRP_INVALID);
        dolly_dlrp_u32(bad.bytes+20+52,DOLLY_WRITE_REJECTED);
        checksum(&bad);
        assert(decode_transfer(bad,&r)==DOLLY_DLRP_READY);
        bad=good;
        unsigned char *target=bad.bytes+12+8+56+8+76+8;
        dolly_dlrp_u64(target+16,UINT64_C(0x100000000));
        dolly_dlrp_u64(target+32,UINT64_C(0x100000800));
        checksum(&bad);
        assert(decode_transfer(bad,&r)==DOLLY_DLRP_READY);
        assert(r.target.endpoint.partition_start_bytes==UINT64_C(0x100000000));
    }
}

int main(void) {
    Buffer out={0};
    unsigned char integer[8];
    static const unsigned char expected[]={
        'F','O','R','M',0,0,0,16,'D','L','R','P',
        'S','R','C','E',0,0,0,3,'D','H','0',0
    };
    assert(!dolly_dlrp_form(write_bytes,&out,11));
    assert(dolly_dlrp_form(write_bytes,&out,12));
    assert(dolly_dlrp_chunk(write_bytes,&out,"SRCE","DH0",3));
    assert(out.length==sizeof(expected) && memcmp(out.bytes,expected,sizeof(expected))==0);
    unsigned char oversized[512]={0};
    assert(!dolly_dlrp_chunk(write_bytes,&out,"HEAD",oversized,sizeof(oversized)));
    dolly_dlrp_u64(integer,UINT64_C(0x0123456789abcdef));
    assert(memcmp(integer,"\x01\x23\x45\x67\x89\xab\xcd\xef",8)==0);
    test_image_reader();
    test_transfer_reader();
    return 0;
}
