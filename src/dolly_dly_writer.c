#include "dolly_dly_writer.h"
#include "dolly_checksum.h"
#include <string.h>

#define DLY_PREFIX_BYTES 60u
#define DLY_CHUNK_OVERHEAD 24u
#define DLY_MAX_DATA_CHUNKS 999998u

static void put32(unsigned char *out,uint32_t value) {
    out[0]=(unsigned char)(value>>24);
    out[1]=(unsigned char)(value>>16);
    out[2]=(unsigned char)(value>>8);
    out[3]=(unsigned char)value;
}
static void put64(unsigned char *out,uint64_t value) {
    put32(out,(uint32_t)(value>>32));
    put32(out+4,(uint32_t)value);
}
int dolly_dly_init(DollyDlyWriter *writer,uint64_t first_lba,uint64_t sectors,
                   uint32_t sector_bytes,uint32_t batch_bytes) {
    uint64_t data_bytes,chunks,file_bytes;
    memset(writer,0,sizeof(*writer));
    if(!sectors || first_lba>UINT64_MAX-(sectors-1u) ||
       sector_bytes<512u || sector_bytes>32768u ||
       (sector_bytes&(sector_bytes-1u)) ||
       batch_bytes<sector_bytes || batch_bytes%sector_bytes ||
       sectors>INT32_MAX/sector_bytes)return 0;
    data_bytes=sectors*sector_bytes;
    chunks=(data_bytes+batch_bytes-1u)/batch_bytes;
    file_bytes=DLY_PREFIX_BYTES+chunks*DLY_CHUNK_OVERHEAD+data_bytes;
    if(chunks>DLY_MAX_DATA_CHUNKS || file_bytes>INT32_MAX)return 0;
    writer->first_lba=first_lba;
    writer->sectors=sectors;
    writer->sector_bytes=sector_bytes;
    writer->batch_bytes=batch_bytes;
    writer->file_bytes=(uint32_t)file_bytes;
    return 1;
}
int dolly_dly_compress(DollyDlyWriter *writer,DollyDlyCodec codec,
                       unsigned char *packed,uint16_t *hash,DollyDlySeek seek) {
    uint64_t bytes,chunks;
    if(!writer->file_bytes || writer->sink || writer->compression ||
       !packed || !seek || writer->batch_bytes>32768u ||
       (codec!=DOLLY_DLY_PACKBITS && (codec!=DOLLY_DLY_LZSS || !hash)))return 0;
    bytes=writer->sectors*writer->sector_bytes;
    chunks=(bytes+writer->batch_bytes-1u)/writer->batch_bytes;
    if(writer->file_bytes+chunks*4u>INT32_MAX)return 0;
    writer->file_bytes+=(uint32_t)(chunks*4u);
    writer->compression=codec;
    writer->packed=packed;
    writer->hash=hash;
    writer->seek=seek;
    return 1;
}
int dolly_dly_begin(DollyDlyWriter *writer,DollyDlySink sink,void *context) {
    unsigned char header[DLY_PREFIX_BYTES]={0};
    if(!writer->file_bytes || writer->sink || !sink)return 0;
    memcpy(header,"FORM",4);
    put32(header+4,writer->file_bytes-8u);
    memcpy(header+8,"DOLYHEAD",8);
    put32(header+16,8);
    put32(header+20,writer->compression?2u:1u);
    memcpy(header+28,"GEOM",4);
    put32(header+32,24);
    put64(header+36,writer->first_lba);
    put64(header+44,writer->sectors);
    put32(header+52,writer->sector_bytes);
    put32(header+56,writer->batch_bytes);
    writer->sink=sink;
    writer->context=context;
    writer->failed=!sink(context,header,sizeof(header));
    writer->emitted=sizeof(header);
    return !writer->failed;
}
int dolly_dly_append(DollyDlyWriter *writer,const void *data,size_t size) {
    unsigned char header[DLY_CHUNK_OVERHEAD+4u];
    const unsigned char *payload=data;
    const unsigned char zero=0;
    uint64_t remaining=(writer->sectors-writer->written)*writer->sector_bytes;
    size_t stored=size,overhead=DLY_CHUNK_OVERHEAD,pad=0;
    DollyDlyCodec codec=DOLLY_DLY_RAW;
    DollyChecksum checksum;
    if(!writer->sink || writer->failed || writer->finished || !data || !size ||
       size!=(remaining<writer->batch_bytes?remaining:writer->batch_bytes))return 0;
    if(writer->compression) {
        size_t limit=size-2u,packed=0;
        overhead+=4u;
        if(writer->compression==DOLLY_DLY_LZSS) {
            packed=dolly_dly_lzss(data,size,writer->packed,limit,writer->hash);
            if(packed) {
                codec=DOLLY_DLY_LZSS;
                limit=packed-1u;
            }
        }
        if(writer->compression==DOLLY_DLY_PACKBITS) {
            packed=dolly_dly_packbits(data,size,writer->packed,limit);
            if(packed)codec=DOLLY_DLY_PACKBITS;
        } else if(dolly_dly_packbits(data,size,NULL,limit)) {
            packed=dolly_dly_packbits(data,size,writer->packed,limit);
            codec=DOLLY_DLY_PACKBITS;
        }
        if(packed) {
            stored=packed;
            payload=writer->packed;
            pad=stored&1u;
        }
        put32(header+24,codec);
    }
    dolly_checksum_init(&checksum);
    if(dolly_checksum_update(&checksum,data,size)!=0)return 0;
    memcpy(header,"CHNK",4);
    put32(header+4,(uint32_t)(stored+overhead-8u));
    put64(header+8,writer->written);
    put32(header+16,(uint32_t)size);
    put32(header+20,dolly_checksum_final(&checksum));
    if(!writer->sink(writer->context,header,overhead) ||
       !writer->sink(writer->context,payload,stored) ||
       (pad && !writer->sink(writer->context,&zero,1))) {
        writer->failed=1;
        return 0;
    }
    writer->emitted+=(uint32_t)(overhead+stored+pad);
    writer->written+=size/writer->sector_bytes;
    return 1;
}
int dolly_dly_finish(DollyDlyWriter *writer) {
    unsigned char size[4];
    if(!writer->sink || writer->failed || writer->written!=writer->sectors)return 0;
    if(writer->finished)return 1;
    if(writer->compression) {
        put32(size,writer->emitted-8u);
        if(!writer->seek(writer->context,4) ||
           !writer->sink(writer->context,size,sizeof(size))) {
            writer->failed=1;
            return 0;
        }
    }
    writer->finished=1;
    return 1;
}
int dolly_dly_complete(const DollyDlyWriter *writer) {
    return writer->sink && !writer->failed && writer->written==writer->sectors &&
           (!writer->compression || writer->finished);
}
