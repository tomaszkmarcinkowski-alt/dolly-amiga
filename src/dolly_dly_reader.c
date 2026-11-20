#include "dolly_dly_reader.h"
#include "dolly_checksum.h"
#include "dolly_dly_codec.h"
#include <stdlib.h>
#include <string.h>

enum {
    PREFIX_BYTES = 60,
    MAX_DATA_CHUNKS = 999998,
    MAX_CACHED_BYTES = 32768,
    STREAM_BYTES = 4096,
    INDEX_STRIDE = 256
};

typedef struct DlyChunk {
    uint32_t payload, stored, raw, crc, next;
    DollyDlyCodec codec;
} DlyChunk;

typedef struct DlyReader {
    DollyBlockSource *container;
    DollyDlyImageInfo info;
    uint32_t file_bytes, decoded_bytes;
    uint32_t cursor_chunk, cursor_offset, cached_chunk;
    uint32_t *index;
    unsigned char *raw, *packed;
} DlyReader;

static uint32_t read32(const unsigned char *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}

static uint64_t read64(const unsigned char *p) {
    return (uint64_t)read32(p)<<32 | read32(p+4);
}

static DollyBlockStatus read_chunk(DlyReader *reader, uint32_t pos, uint32_t number,
                                   DlyChunk *chunk) {
    unsigned char header[28], pad;
    uint32_t header_bytes=reader->info.version==1?24u:28u;
    uint32_t start=number*reader->info.batch_bytes;
    uint32_t remaining=reader->decoded_bytes-start;
    uint32_t length,codec;
    DollyBlockStatus status;
    if(pos>reader->file_bytes || reader->file_bytes-pos<header_bytes)
        return DOLLY_BLOCK_EINVAL;
    status=dolly_block_read(reader->container,pos,header,header_bytes).status;
    if(status!=DOLLY_BLOCK_OK)return status;
    length=read32(header+4);
    if(memcmp(header,"CHNK",4) || length<=header_bytes-8u ||
       length>reader->file_bytes-pos-8u ||
       (length&1u)>reader->file_bytes-pos-8u-length ||
       read64(header+8)!=start/reader->info.sector_bytes)
        return DOLLY_BLOCK_EINVAL;
    chunk->raw=read32(header+16);
    if(chunk->raw!=(remaining<reader->info.batch_bytes?remaining:reader->info.batch_bytes))
        return DOLLY_BLOCK_EINVAL;
    codec=reader->info.version==1?0:read32(header+24);
    if(codec>DOLLY_DLY_LZSS)return DOLLY_BLOCK_EUNSUPPORTED;
    chunk->codec=(DollyDlyCodec)codec;
    chunk->stored=length-(header_bytes-8u);
    if(codec==DOLLY_DLY_RAW ? chunk->stored!=chunk->raw : chunk->stored>=chunk->raw)
        return DOLLY_BLOCK_EINVAL;
    chunk->payload=pos+header_bytes;
    chunk->next=pos+8u+length+(length&1u);
    chunk->crc=read32(header+20);
    if(length&1u) {
        status=dolly_block_read(reader->container,chunk->next-1u,&pad,1).status;
        if(status!=DOLLY_BLOCK_OK)return status;
        if(pad)return DOLLY_BLOCK_EINVAL;
    }
    return DOLLY_BLOCK_OK;
}

static DollyBlockStatus locate_chunk(DlyReader *reader, uint32_t number, DlyChunk *chunk) {
    uint32_t current=number-number%INDEX_STRIDE;
    uint32_t pos=reader->index[number/INDEX_STRIDE];
    DollyBlockStatus status;
    if(reader->cursor_chunk>=current && reader->cursor_chunk<=number) {
        current=reader->cursor_chunk;
        pos=reader->cursor_offset;
    }
    do {
        status=read_chunk(reader,pos,current,chunk);
        if(status!=DOLLY_BLOCK_OK)return status;
        pos=chunk->next;
    } while(current++<number);
    reader->cursor_chunk=current;
    reader->cursor_offset=pos;
    return DOLLY_BLOCK_OK;
}

static DollyBlockStatus load_slice(DlyReader *reader, const DlyChunk *chunk,
                                  uint32_t offset, unsigned char *out, size_t bytes) {
    DollyBlockStatus status;
    DollyChecksum crc;
    dolly_checksum_init(&crc);
    if(reader->info.batch_bytes>MAX_CACHED_BYTES) {
        uint32_t pos=0,end=offset+(uint32_t)bytes;
        /* Only v1 permits chunks above the cache budget. Validate the full
         * raw chunk in small reads, copying just the requested slice. */
        while(pos<chunk->raw) {
            uint32_t length=chunk->raw-pos;
            uint32_t first,last;
            if(length>STREAM_BYTES)length=STREAM_BYTES;
            status=dolly_block_read(reader->container,chunk->payload+pos,reader->raw,length).status;
            if(status!=DOLLY_BLOCK_OK)return status;
            dolly_checksum_update(&crc,reader->raw,length);
            first=pos>offset?pos:offset;
            last=pos+length<end?pos+length:end;
            if(first<last)memcpy(out+(first-offset),reader->raw+(first-pos),last-first);
            pos+=length;
        }
    } else {
        unsigned char *stored=chunk->codec==DOLLY_DLY_RAW?reader->raw:reader->packed;
        status=dolly_block_read(reader->container,chunk->payload,stored,chunk->stored).status;
        if(status!=DOLLY_BLOCK_OK)return status;
        if(chunk->codec!=DOLLY_DLY_RAW &&
           !dolly_dly_decode(chunk->codec,stored,chunk->stored,reader->raw,chunk->raw))
            return DOLLY_BLOCK_EIO;
        dolly_checksum_update(&crc,reader->raw,chunk->raw);
    }
    if(dolly_checksum_final(&crc)!=chunk->crc)return DOLLY_BLOCK_EIO;
    if(reader->info.batch_bytes<=MAX_CACHED_BYTES)memcpy(out,reader->raw+offset,bytes);
    return DOLLY_BLOCK_OK;
}

static DollyBlockReadResult read_decoded(void *context, uint64_t offset,
                                        void *buffer, size_t bytes) {
    DlyReader *reader=context;
    DollyBlockReadResult result={DOLLY_BLOCK_OK,0};
    unsigned char *out=buffer;
    while(result.bytes_read<bytes) {
        uint32_t pos=(uint32_t)(offset+result.bytes_read);
        uint32_t number=pos/reader->info.batch_bytes;
        uint32_t inside=pos%reader->info.batch_bytes;
        size_t amount=reader->info.batch_bytes-inside;
        if(amount>bytes-result.bytes_read)amount=bytes-result.bytes_read;
        if(reader->cached_chunk==number)memcpy(out+result.bytes_read,reader->raw+inside,amount);
        else {
            DlyChunk chunk;
            reader->cached_chunk=UINT32_MAX;
            result.status=locate_chunk(reader,number,&chunk);
            if(result.status==DOLLY_BLOCK_OK)
                result.status=load_slice(reader,&chunk,inside,out+result.bytes_read,amount);
            if(result.status!=DOLLY_BLOCK_OK)break;
            if(reader->info.batch_bytes<=MAX_CACHED_BYTES)reader->cached_chunk=number;
        }
        result.bytes_read+=amount;
    }
    return result;
}

static void free_reader(DlyReader *reader) {
    free(reader->index);
    free(reader->raw);
    free(reader->packed);
    free(reader);
}

static void close_decoded(void *context) {
    DlyReader *reader=context;
    dolly_block_close(reader->container);
    free_reader(reader);
}

DollyBlockStatus dolly_dly_open(DollyBlockSource *container, DollyBlockSource **out,
                               DollyDlyImageInfo *info) {
    unsigned char header[PREFIX_BYTES];
    DlyReader *reader;
    DollyDlyImageInfo parsed;
    DollyBlockStatus status;
    uint64_t size=dolly_block_size(container);
    uint32_t total,count,number,pos=PREFIX_BYTES;
    if(!out)return DOLLY_BLOCK_EINVAL;
    *out=NULL;
    if(!container || size<PREFIX_BYTES || size>INT32_MAX)return DOLLY_BLOCK_EINVAL;
    status=dolly_block_read(container,0,header,sizeof(header)).status;
    if(status!=DOLLY_BLOCK_OK)return status;
    if(memcmp(header,"FORM",4) || read32(header+4)!=size-8u ||
       memcmp(header+8,"DOLYHEAD",8) || read32(header+16)!=8 ||
       memcmp(header+28,"GEOM",4) || read32(header+32)!=24)
        return DOLLY_BLOCK_EINVAL;
    parsed=(DollyDlyImageInfo){read64(header+36),read64(header+44),read32(header+52),
                               read32(header+56),read32(header+20)};
    if((parsed.version!=1 && parsed.version!=2) || read32(header+24))
        return DOLLY_BLOCK_EUNSUPPORTED;
    if(!parsed.sectors || parsed.first_lba>UINT64_MAX-(parsed.sectors-1u) ||
       parsed.sector_bytes<512u || parsed.sector_bytes>32768u ||
       (parsed.sector_bytes&(parsed.sector_bytes-1u)) ||
       parsed.batch_bytes<parsed.sector_bytes || parsed.batch_bytes%parsed.sector_bytes ||
       parsed.sectors>INT32_MAX/parsed.sector_bytes ||
       (parsed.version==2 && parsed.batch_bytes>MAX_CACHED_BYTES))
        return DOLLY_BLOCK_EINVAL;
    total=(uint32_t)(parsed.sectors*parsed.sector_bytes);
    count=(total-1u)/parsed.batch_bytes+1u;
    if(count>MAX_DATA_CHUNKS || count>(size-PREFIX_BYTES)/24u)
        return DOLLY_BLOCK_EINVAL;
    reader=calloc(1,sizeof(*reader));
    if(!reader)return DOLLY_BLOCK_ENOMEM;
    reader->container=container;
    reader->info=parsed;
    reader->file_bytes=(uint32_t)size;
    reader->decoded_bytes=total;
    reader->index=malloc(((count-1u)/INDEX_STRIDE+1u)*sizeof(*reader->index));
    if(!reader->index)goto no_memory;
    for(number=0;number<count;++number) {
        DlyChunk chunk;
        if(number%INDEX_STRIDE==0)reader->index[number/INDEX_STRIDE]=pos;
        status=read_chunk(reader,pos,number,&chunk);
        if(status!=DOLLY_BLOCK_OK)goto fail;
        pos=chunk.next;
    }
    if(pos!=size) {
        status=DOLLY_BLOCK_EINVAL;
        goto fail;
    }
    reader->raw=malloc(parsed.batch_bytes>MAX_CACHED_BYTES?STREAM_BYTES:parsed.batch_bytes);
    if(parsed.version==2)reader->packed=malloc(parsed.batch_bytes);
    if(!reader->raw || (parsed.version==2 && !reader->packed))goto no_memory;
    reader->cursor_offset=PREFIX_BYTES;
    reader->cached_chunk=UINT32_MAX;
    status=dolly_block_open_backend(total,read_decoded,close_decoded,reader,out);
    if(status!=DOLLY_BLOCK_OK)goto fail;
    if(info)*info=parsed;
    return status;
no_memory:
    status=DOLLY_BLOCK_ENOMEM;
fail:
    free_reader(reader);
    return status;
}
