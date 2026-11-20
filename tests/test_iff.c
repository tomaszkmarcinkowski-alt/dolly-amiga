#define _POSIX_C_SOURCE 200809L
#include "dolly_iff.h"
#include "dolly_dly_writer.h"
#include "dolly_dly_reader.h"
#include "dolly_restore_transfer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void wr32(unsigned char *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static DollyIffStatus probe(const unsigned char *p,size_t n,unsigned *chunks){
 char name[]="/tmp/dolly-iff-XXXXXX";int fd=mkstemp(name);FILE *f;
 DollyBlockSource *src=NULL;DollyIffSummary sum;DollyIffStatus status;
 assert(fd>=0);f=fdopen(fd,"wb");assert(f);
 assert(fwrite(p,1,n,f)==n);assert(fclose(f)==0);
 assert(dolly_block_open_file(name,&src)==DOLLY_BLOCK_OK);
 status=dolly_iff_scan(src,NULL,NULL,&sum);*chunks=sum.chunk_count;
 dolly_block_close(src);assert(unlink(name)==0);return status;
}
typedef struct Buffer {
    unsigned char bytes[144 * 1024];
    size_t used,limit,position;
} Buffer;

static int append(void *context,const void *data,size_t size) {
    Buffer *buffer=context;
    if(size>buffer->limit-buffer->position)return 0;
    memcpy(buffer->bytes+buffer->position,data,size);
    buffer->position+=size;
    if(buffer->position>buffer->used)buffer->used=buffer->position;
    return 1;
}
static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static DollyBlockReadResult read_buffer(void *context,uint64_t pos,void *out,size_t bytes) {
    Buffer *buffer=context;
    assert(pos<=buffer->used && bytes<=buffer->used-pos);
    memcpy(out,buffer->bytes+pos,bytes);
    return (DollyBlockReadResult){DOLLY_BLOCK_OK,bytes};
}
static void close_buffer(void *context) { (void)context; }

static DollyBlockStatus open_image(Buffer *buffer,DollyBlockSource **decoded,
                                   DollyDlyImageInfo *info) {
    DollyBlockSource *container=NULL;
    DollyBlockStatus status;
    assert(dolly_block_open_backend(buffer->used,read_buffer,close_buffer,buffer,
                                   &container)==DOLLY_BLOCK_OK);
    status=dolly_dly_open(container,decoded,info);
    if(status!=DOLLY_BLOCK_OK)dolly_block_close(container);
    return status;
}

static void check_decoded(Buffer *buffer,const unsigned char *raw,size_t size,
                          unsigned version,uint64_t first_lba) {
    DollyBlockSource *decoded=NULL;
    DollyDlyImageInfo info;
    unsigned char bytes[1536];
    assert(size<=sizeof(bytes));
    assert(open_image(buffer,&decoded,&info)==DOLLY_BLOCK_OK);
    assert(info.version==version && info.first_lba==first_lba && info.sector_bytes==512);
    assert(info.sectors*512==size && dolly_block_size(decoded)==size);
    assert(dolly_block_read(decoded,0,bytes,size).status==DOLLY_BLOCK_OK);
    assert(!memcmp(bytes,raw,size));
    assert(dolly_block_read(decoded,499,bytes,size-499).status==DOLLY_BLOCK_OK);
    assert(!memcmp(bytes,raw+499,size-499));
    assert(dolly_block_read(decoded,size-3,bytes,4).status==DOLLY_BLOCK_ESHORT);
    assert(!memcmp(bytes,raw+size-3,3));
    dolly_block_close(decoded);
}

static void verify_writer(void) {
    DollyDlyWriter writer;
    Buffer buffer={.limit=2048};
    unsigned char payload[1536];
    unsigned chunks,i;
    for(i=0;i<sizeof(payload);++i)payload[i]=(unsigned char)i;
    /* Also exercise preservation of absolute LBAs larger than 32 bits. */
    assert(dolly_dly_init(&writer,UINT64_C(0x100000003),3,512,1024));
    assert(dolly_dly_begin(&writer,append,&buffer));
    assert(!dolly_dly_complete(&writer));
    assert(!dolly_dly_append(&writer,payload,512));
    assert(dolly_dly_append(&writer,payload,1024));
    assert(dolly_dly_append(&writer,payload+1024,512));
    assert(dolly_dly_complete(&writer));
    assert(!dolly_dly_append(&writer,payload,512));
    assert(buffer.used==1644 && probe(buffer.bytes,buffer.used,&chunks)==DOLLY_IFF_OK);
    assert(chunks==4);
    check_decoded(&buffer,payload,sizeof(payload),1,UINT64_C(0x100000003));
    assert(!memcmp(buffer.bytes+12,"HEAD",4) && rd32(buffer.bytes+20)==1);
    assert(!memcmp(buffer.bytes+28,"GEOM",4));
    assert(rd32(buffer.bytes+36)==1 && rd32(buffer.bytes+40)==3);
    assert(rd32(buffer.bytes+48)==3 && rd32(buffer.bytes+52)==512);
    assert(!memcmp(buffer.bytes+60,"CHNK",4) && rd32(buffer.bytes+64)==1040);
    assert(rd32(buffer.bytes+76)==1024 && rd32(buffer.bytes+80)==0xb70b4c26u);
    assert(!memcmp(buffer.bytes+84,payload,1024));
    assert(!memcmp(buffer.bytes+1108,"CHNK",4) && rd32(buffer.bytes+1112)==528);
    assert(rd32(buffer.bytes+1120)==2 && rd32(buffer.bytes+1124)==512);
    assert(rd32(buffer.bytes+1128)==0x1c613576u);
    assert(!memcmp(buffer.bytes+1132,payload+1024,512));

    buffer.used=buffer.position=0;
    buffer.limit=84; /* fails the payload write after the chunk header */
    assert(dolly_dly_init(&writer,0,1,512,1024));
    assert(dolly_dly_begin(&writer,append,&buffer));
    assert(!dolly_dly_append(&writer,payload,512));
    assert(!dolly_dly_complete(&writer));
    buffer.limit=2048;
    assert(!dolly_dly_append(&writer,payload,512));
    assert(!dolly_dly_init(&writer,0,INT32_MAX/512,512,16384));
    assert(!dolly_dly_init(&writer,UINT64_MAX,2,512,1024));
    assert(!dolly_dly_init(&writer,0,2,512,1000));
}
static int seek(void *context,uint32_t offset) {
    Buffer *buffer=context;
    if(offset>buffer->used || buffer->limit==0)return 0;
    buffer->position=offset;
    return 1;
}
static void verify_compression(void) {
    DollyDlyWriter writer;
    unsigned char data[1024],decoded[512],packed[512];
    uint16_t hash[DOLLY_DLY_HASH_SLOTS];
    unsigned mode,i,chunks;
    uint32_t random=123456789u;
    memset(data,0,512);
    data[0]=1;
    data[1]=2; /* gives the first PackBits payload an odd length */
    for(i=512;i<sizeof(data);++i) {
        random^=random<<13;
        random^=random>>17;
        random^=random<<5;
        data[i]=(unsigned char)random;
    }
    for(mode=DOLLY_DLY_PACKBITS;mode<=DOLLY_DLY_LZSS;++mode) {
        Buffer buffer={.limit=2048};
        size_t pos=60;
        assert(dolly_dly_init(&writer,99,2,512,512));
        assert(dolly_dly_compress(&writer,(DollyDlyCodec)mode,packed,hash,seek));
        assert(dolly_dly_begin(&writer,append,&buffer));
        assert(!dolly_dly_finish(&writer));
        assert(dolly_dly_append(&writer,data,512));
        assert(dolly_dly_append(&writer,data+512,512));
        assert(!dolly_dly_complete(&writer));
        assert(dolly_dly_finish(&writer) && dolly_dly_complete(&writer));
        assert(dolly_dly_finish(&writer));
        assert(rd32(buffer.bytes+4)==buffer.used-8);
        assert(rd32(buffer.bytes+20)==2);
        assert(probe(buffer.bytes,buffer.used,&chunks)==DOLLY_IFF_OK && chunks==4);
        for(i=0;i<2;++i) {
            size_t length=rd32(buffer.bytes+pos+4);
            DollyDlyCodec codec=(DollyDlyCodec)rd32(buffer.bytes+pos+24);
            assert(codec==(i==0?DOLLY_DLY_PACKBITS:DOLLY_DLY_RAW));
            if(i==0)assert((length&1u) && buffer.bytes[pos+8+length]==0);
            assert(dolly_dly_decode(codec,buffer.bytes+pos+28,length-20,
                                   decoded,sizeof(decoded)));
            assert(memcmp(decoded,data+i*512,512)==0);
            pos+=8+length+(length&1u);
        }
        assert(pos==buffer.used);
        check_decoded(&buffer,data,sizeof(data),2,99);
        if(mode==DOLLY_DLY_PACKBITS) {
            DollyBlockSource *decoded=NULL;
            unsigned char output[1024];
            size_t first_length=rd32(buffer.bytes+64);
            size_t second=68+first_length+(first_length&1u);
            buffer.bytes[68+first_length]=1; /* Invalid odd pad. */
            assert(open_image(&buffer,&decoded,NULL)==DOLLY_BLOCK_EINVAL && !decoded);
            buffer.bytes[68+first_length]=0;
            buffer.bytes[second+15]^=1; /* Gap in relative sectors. */
            assert(open_image(&buffer,&decoded,NULL)==DOLLY_BLOCK_EINVAL && !decoded);
            buffer.bytes[second+15]^=1;
            buffer.bytes[second+27]=99;
            assert(open_image(&buffer,&decoded,NULL)==DOLLY_BLOCK_EUNSUPPORTED && !decoded);
            buffer.bytes[second+27]=0;
            buffer.bytes[second+28]^=1; /* Corrupt the raw second chunk. */
            assert(open_image(&buffer,&decoded,NULL)==DOLLY_BLOCK_OK);
            {
                DollyBlockReadResult result=dolly_block_read(decoded,0,output,sizeof(output));
                assert(result.status==DOLLY_BLOCK_EIO && result.bytes_read==512);
                assert(!memcmp(output,data,512));
            }
            dolly_block_close(decoded);
            --buffer.used;
            assert(open_image(&buffer,&decoded,NULL)==DOLLY_BLOCK_EINVAL && !decoded);
        }
    }
    for(i=0;i<512;++i)data[i]=(unsigned char)(i%31u);
    {
        Buffer buffer={.limit=2048};
        size_t length;
        assert(dolly_dly_init(&writer,0,1,512,512));
        assert(dolly_dly_compress(&writer,DOLLY_DLY_LZSS,packed,hash,seek));
        assert(dolly_dly_begin(&writer,append,&buffer));
        assert(dolly_dly_append(&writer,data,512));
        assert(rd32(buffer.bytes+84)==DOLLY_DLY_LZSS);
        length=rd32(buffer.bytes+64)-20u;
        assert(dolly_dly_decode(DOLLY_DLY_LZSS,buffer.bytes+88,length,decoded,512));
        assert(memcmp(data,decoded,512)==0);
        assert(!dolly_dly_decode(DOLLY_DLY_LZSS,buffer.bytes+88,length-1,decoded,512));
        assert(!dolly_dly_decode(DOLLY_DLY_LZSS,(const unsigned char *)"\1\0\0",3,decoded,3));
        assert(dolly_dly_finish(&writer));
        check_decoded(&buffer,data,512,2,0);
        buffer.used=buffer.position=0;
        assert(dolly_dly_init(&writer,0,1,512,512));
        assert(dolly_dly_compress(&writer,DOLLY_DLY_LZSS,packed,hash,seek));
        assert(dolly_dly_begin(&writer,append,&buffer));
        assert(dolly_dly_append(&writer,data,512));
        buffer.limit=0; /* failed FORM size fix-up cannot publish an image */
        assert(!dolly_dly_finish(&writer) && !dolly_dly_complete(&writer));
    }
}
static void verify_reader_navigation(void) {
    static Buffer buffer;
    DollyBlockSource *decoded=NULL;
    DollyDlyWriter writer;
    unsigned char sector[512],packed[512],actual[512];
    unsigned i;
    assert(dolly_dly_init(&writer,0,260,512,512));
    assert(dolly_dly_compress(&writer,DOLLY_DLY_PACKBITS,packed,NULL,seek));
    buffer.limit=sizeof(buffer.bytes);
    assert(dolly_dly_begin(&writer,append,&buffer));
    for(i=0;i<260;++i) {
        memset(sector,i&255u,sizeof(sector));
        sector[0]=(unsigned char)(i>>8);
        assert(dolly_dly_append(&writer,sector,sizeof(sector)));
    }
    assert(dolly_dly_finish(&writer));
    assert(open_image(&buffer,&decoded,NULL)==DOLLY_BLOCK_OK);
    assert(dolly_block_read(decoded,259u*512u,actual,sizeof(actual)).status==DOLLY_BLOCK_OK);
    assert(!memcmp(actual,sector,sizeof(actual)));
    assert(dolly_block_read(decoded,255u*512u,actual,sizeof(actual)).status==DOLLY_BLOCK_OK);
    assert(actual[0]==0 && actual[511]==255);
    assert(dolly_block_read(decoded,256u*512u,actual,sizeof(actual)).status==DOLLY_BLOCK_OK);
    assert(actual[0]==1 && actual[511]==0);
    dolly_block_close(decoded);
}
static void verify_large_raw_read(void) {
    static Buffer buffer;
    static unsigned char raw[65536];
    unsigned char slice[512];
    DollyBlockSource *decoded=NULL;
    DollyDlyWriter writer;
    memset(raw+40000,0x5a,sizeof(raw)-40000);
    buffer.limit=sizeof(buffer.bytes);
    assert(dolly_dly_init(&writer,0,128,512,sizeof(raw)));
    assert(dolly_dly_begin(&writer,append,&buffer));
    assert(dolly_dly_append(&writer,raw,sizeof(raw)));
    assert(dolly_dly_finish(&writer));
    assert(open_image(&buffer,&decoded,NULL)==DOLLY_BLOCK_OK);
    assert(dolly_block_read(decoded,39900,slice,sizeof(slice)).status==DOLLY_BLOCK_OK);
    assert(!memcmp(slice,raw+39900,sizeof(slice)));
    dolly_block_close(decoded);
    buffer.bytes[buffer.used-1]^=1;
    assert(open_image(&buffer,&decoded,NULL)==DOLLY_BLOCK_OK);
    {
        DollyBlockReadResult result=dolly_block_read(decoded,0,slice,sizeof(slice));
        assert(result.status==DOLLY_BLOCK_EIO && result.bytes_read==0);
    }
    dolly_block_close(decoded);
}
static DollyBlockWriteStatus write_restored_sector(void *context,uint64_t offset,
                                                  const void *data,size_t size,int verify) {
    Buffer *output=context;
    assert(offset==output->used && verify);
    assert(append(output,data,size));
    return DOLLY_WRITE_VERIFIED;
}
static void verify_restore_gate(void) {
    static Buffer image,output;
    DollyBlockSource *decoded=NULL;
    DollyDlyWriter writer;
    DollyRestoreTransfer transfer;
    unsigned char raw[1024],packed[512],scratch[512];
    unsigned corrupt;
    memset(raw,0x11,512);
    memset(raw+512,0x22,512);
    image.limit=sizeof(image.bytes);
    output.limit=sizeof(output.bytes);
    assert(dolly_dly_init(&writer,0,2,512,512));
    assert(dolly_dly_compress(&writer,DOLLY_DLY_PACKBITS,packed,NULL,seek));
    assert(dolly_dly_begin(&writer,append,&image));
    assert(dolly_dly_append(&writer,raw,512));
    assert(dolly_dly_append(&writer,raw+512,512));
    assert(dolly_dly_finish(&writer));
    for(corrupt=0;corrupt<=1;++corrupt) {
        if(corrupt)image.bytes[image.used-1]^=1;
        output.used=output.position=0;
        assert(open_image(&image,&decoded,NULL)==DOLLY_BLOCK_OK);
        assert(dolly_restore_transfer_init_prefix(&transfer,decoded,512,512,512,&output,
                                                  write_restored_sector,1));
        transfer.source_check_bytes=dolly_block_size(decoded);
        assert(dolly_restore_transfer_step(&transfer,scratch,sizeof(scratch))==
               DOLLY_RESTORE_TRANSFERRING);
        assert(transfer.source_checked==512 && transfer.processed==0 && output.used==0);
        if(corrupt) {
            /* Validate even the tail excluded from a truncated restore. */
            assert(dolly_restore_transfer_step(&transfer,scratch,sizeof(scratch))==
                   DOLLY_RESTORE_READ_FAILED);
            assert(transfer.read_status==DOLLY_BLOCK_EIO && output.used==0);
        } else {
            assert(dolly_restore_transfer_step(&transfer,scratch,sizeof(scratch))==
                   DOLLY_RESTORE_TRANSFERRING);
            assert(transfer.source_checked==sizeof(raw) && output.used==0);
            assert(dolly_restore_transfer_step(&transfer,scratch,sizeof(scratch))==
                   DOLLY_RESTORE_FINISHED);
            assert(output.used==512 && !memcmp(output.bytes,raw,512));
        }
        dolly_block_close(decoded);
    }
}
int main(void){
 unsigned char p[128]={0};unsigned chunks;unsigned i;
 memcpy(p,"FORM",4);wr32(p+4,4);memcpy(p+8,"DOLY",4);
 assert(probe(p,12,&chunks)==DOLLY_IFF_OK&&chunks==0);
 memcpy(p+12,"ANNO",4);wr32(p+16,3);memcpy(p+20,"abc",3);p[23]=0xfe;
 wr32(p+4,16);assert(probe(p,24,&chunks)==DOLLY_IFF_OK&&chunks==1);
 assert(probe(p,23,&chunks)==DOLLY_IFF_BAD_SIZE);
 wr32(p+16,4);assert(probe(p,24,&chunks)==DOLLY_IFF_OK);
 wr32(p+16,5);assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_CHUNK);
 wr32(p+16,0xffffffffu);assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_SIZE);
 wr32(p+16,0);wr32(p+4,16);
 assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_CHUNK);
 wr32(p+4,0xffffffffu);assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_SIZE);
 wr32(p+4,16);p[8]='X';assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_FORM);p[8]='D';
 p[0]='X';assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_FORM);p[0]='F';
 assert(probe(p,5,&chunks)==DOLLY_IFF_TRUNCATED);
 for(i=0;i<256;i++){
  p[4]=(unsigned char)i;p[5]=(unsigned char)(i*3);
  {DollyIffStatus s=probe(p,24,&chunks);assert(s!=DOLLY_IFF_IO_ERROR);}
 }
 verify_writer();
 verify_compression();
 verify_reader_navigation();
 verify_large_raw_read();
 verify_restore_gate();
 puts("IFF structural and DLY writer tests passed");return 0;
}
