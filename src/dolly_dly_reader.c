#include "dolly_dly_reader.h"
#include "dolly_checksum.h"
#include "dolly_dly_codec.h"
#include <stdlib.h>
#include <string.h>

enum {
    PREFIX_BYTES = DOLLY_DLY_GEOM_END,
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
    uint32_t chunk_count,indexed_chunks,index_offset;
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

static DollyBlockStatus extend_index(DlyReader *reader,uint32_t number,DlyChunk *chunk) {
    while(reader->indexed_chunks<=number) {
        uint32_t current=reader->indexed_chunks;
        uint32_t pos=reader->index_offset;
        DollyBlockStatus status=read_chunk(reader,pos,current,chunk);
        if(status!=DOLLY_BLOCK_OK)return status;
        if(current+1u==reader->chunk_count && chunk->next!=reader->file_bytes)
            return DOLLY_BLOCK_EINVAL;
        if(current%INDEX_STRIDE==0)reader->index[current/INDEX_STRIDE]=pos;
        reader->index_offset=chunk->next;
        ++reader->indexed_chunks;
    }
    return DOLLY_BLOCK_OK;
}

static DollyBlockStatus locate_chunk(DlyReader *reader, uint32_t number, DlyChunk *chunk) {
    uint32_t current,pos;
    DollyBlockStatus status;
    if(number>=reader->indexed_chunks) {
        status=extend_index(reader,number,chunk);
        if(status==DOLLY_BLOCK_OK) {
            reader->cursor_chunk=number+1u;
            reader->cursor_offset=chunk->next;
        }
        return status;
    }
    current=number-number%INDEX_STRIDE;
    pos=reader->index[number/INDEX_STRIDE];
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

static int valid_set(const DollyDlySetInfo *set) {
    uint32_t sector=set->sector_bytes;
    return sector>=512u && sector<=32768u && !(sector&(sector-1u)) &&
           set->sectors && set->sectors<=UINT64_MAX/sector &&
           set->first_lba<=UINT64_MAX-(set->sectors-1u) &&
           set->sectors_per_part && set->sectors_per_part<=INT32_MAX/sector &&
           set->parts && set->parts==(set->sectors-1u)/set->sectors_per_part+1u;
}

DollyBlockStatus dolly_dly_set_unpack(const unsigned char *prefix,size_t bytes,
                                     DollyDlySetInfo *out) {
    const unsigned char *chunk;
    unsigned char payload[DOLLY_DLY_NUMBERED_MSET_BYTES-8u];
    uint32_t length,version;
    DollyDlyImageInfo part={0};
    DollyChecksum checksum;
    DollyDlySetInfo set;
    if(!prefix || !out || bytes<PREFIX_BYTES+DOLLY_DLY_MSET_BYTES ||
       memcmp(prefix,"FORM",4) || memcmp(prefix+8,"DOLYHEAD",8) ||
       read32(prefix+16)!=8 || memcmp(prefix+28,"GEOM",4) ||
       read32(prefix+32)!=24)return DOLLY_BLOCK_EINVAL;
    chunk=prefix+PREFIX_BYTES;
    if(memcmp(chunk,"MSET",4))return DOLLY_BLOCK_EINVAL;
    version=read32(chunk+8);
    length=version==2u?DOLLY_DLY_NUMBERED_MSET_BYTES:DOLLY_DLY_MSET_BYTES;
    if((version!=1u && version!=2u) || bytes<PREFIX_BYTES+length ||
       read32(chunk+4)!=length-8u)return DOLLY_BLOCK_EINVAL;
    memcpy(payload,chunk+8,length-8u);
    memset(payload+12,0,4);
    dolly_checksum_init(&checksum);
    if(dolly_checksum_update(&checksum,payload,length-8u)!=0 ||
       dolly_checksum_final(&checksum)!=read32(chunk+20))return DOLLY_BLOCK_EIO;
    set=(DollyDlySetInfo){read64(chunk+24),read64(chunk+32),read32(prefix+52),
                          read32(chunk+12),read32(chunk+16),0,0};
    if(version==2u) {
        set.part=read32(chunk+40);
        set.set_id=read64(chunk+44);
        if(!set.part || !set.set_id)return DOLLY_BLOCK_EINVAL;
    }
    part.first_lba=read64(prefix+36);
    part.sectors=read64(prefix+44);
    part.sector_bytes=read32(prefix+52);
    if(!valid_set(&set) || set.parts<2u ||
       !dolly_dly_set_matches(&set,set.part?set.part-1u:0,&part))return DOLLY_BLOCK_EINVAL;
    *out=set;
    return DOLLY_BLOCK_OK;
}

static DollyBlockStatus read_header(DollyBlockSource *container,DollyDlyImageInfo *info,
                                   uint32_t *start,uint32_t *chunks) {
    unsigned char header[PREFIX_BYTES+DOLLY_DLY_NUMBERED_MSET_BYTES];
    DollyDlyImageInfo parsed;
    DollyBlockStatus status;
    uint64_t size=dolly_block_size(container);
    uint32_t total,count,data_start=PREFIX_BYTES;
    size_t prefix_bytes;
    if(!container || size<PREFIX_BYTES || size>INT32_MAX)return DOLLY_BLOCK_EINVAL;
    prefix_bytes=size<sizeof(header)?(size_t)size:sizeof(header);
    status=dolly_block_read(container,0,header,prefix_bytes).status;
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
    if(prefix_bytes>=PREFIX_BYTES+4u && !memcmp(header+PREFIX_BYTES,"MSET",4)) {
        DollyDlySetInfo set;
        status=dolly_dly_set_unpack(header,prefix_bytes,&set);
        if(status!=DOLLY_BLOCK_OK)return status;
        data_start+=set.part?DOLLY_DLY_NUMBERED_MSET_BYTES:DOLLY_DLY_MSET_BYTES;
    }
    total=(uint32_t)(parsed.sectors*parsed.sector_bytes);
    count=(total-1u)/parsed.batch_bytes+1u;
    if(count>MAX_DATA_CHUNKS || count>(size-data_start)/24u)
        return DOLLY_BLOCK_EINVAL;
    *info=parsed;
    *start=data_start;
    *chunks=count;
    return DOLLY_BLOCK_OK;
}

DollyBlockStatus dolly_dly_probe_header(DollyBlockSource *container,DollyDlyImageInfo *info) {
    uint32_t start,chunks;
    if(!info)return DOLLY_BLOCK_EINVAL;
    return read_header(container,info,&start,&chunks);
}

static DollyBlockStatus open_reader(DollyBlockSource *container,DollyBlockSource **out,
                                    DollyDlyImageInfo *info,int lazy) {
    DlyReader *reader;
    DollyDlyImageInfo parsed;
    DollyBlockStatus status;
    uint32_t count,data_start;
    if(!out)return DOLLY_BLOCK_EINVAL;
    *out=NULL;
    status=read_header(container,&parsed,&data_start,&count);
    if(status!=DOLLY_BLOCK_OK)return status;
    reader=calloc(1,sizeof(*reader));
    if(!reader)return DOLLY_BLOCK_ENOMEM;
    reader->container=container;
    reader->info=parsed;
    reader->file_bytes=(uint32_t)dolly_block_size(container);
    reader->decoded_bytes=(uint32_t)(parsed.sectors*parsed.sector_bytes);
    reader->chunk_count=count;
    reader->index_offset=data_start;
    reader->index=malloc(((count-1u)/INDEX_STRIDE+1u)*sizeof(*reader->index));
    if(!reader->index)goto no_memory;
    if(!lazy) {
        DlyChunk chunk;
        status=extend_index(reader,count-1u,&chunk);
        if(status!=DOLLY_BLOCK_OK)goto fail;
    }
    reader->raw=malloc(parsed.batch_bytes>MAX_CACHED_BYTES?STREAM_BYTES:parsed.batch_bytes);
    if(parsed.version==2)reader->packed=malloc(parsed.batch_bytes);
    if(!reader->raw || (parsed.version==2 && !reader->packed))goto no_memory;
    reader->cursor_offset=data_start;
    reader->cached_chunk=UINT32_MAX;
    status=dolly_block_open_backend(reader->decoded_bytes,read_decoded,close_decoded,reader,out);
    if(status!=DOLLY_BLOCK_OK)goto fail;
    if(info)*info=parsed;
    return status;
no_memory:
    status=DOLLY_BLOCK_ENOMEM;
fail:
    free_reader(reader);
    return status;
}

DollyBlockStatus dolly_dly_open(DollyBlockSource *container,DollyBlockSource **out,
                               DollyDlyImageInfo *info) {
    return open_reader(container,out,info,0);
}

DollyBlockStatus dolly_dly_open_lazy(DollyBlockSource *container,DollyBlockSource **out,
                                    DollyDlyImageInfo *info) {
    return open_reader(container,out,info,1);
}

static int manifest_literal(const char **cursor,const char *end,const char *literal) {
    size_t length=strlen(literal);
    if((size_t)(end-*cursor)<length || memcmp(*cursor,literal,length))return 0;
    *cursor+=length;
    return 1;
}

static int manifest_number(const char **cursor,const char *end,const char *label,
                            uint64_t *out) {
    const char *p;
    uint64_t value=0;
    if(!manifest_literal(cursor,end,label))return 0;
    p=*cursor;
    if(p==end || *p<'0' || *p>'9')return 0;
    while(p!=end && *p>='0' && *p<='9') {
        unsigned digit=(unsigned)(*p-'0');
        if(value>(UINT64_MAX-digit)/10u)return 0;
        value=value*10u+digit;
        ++p;
    }
    if(p==end || *p!='\n')return 0;
    *cursor=p+1;
    *out=value;
    return 1;
}

DollyBlockStatus dolly_dly_set_parse(const char *text,size_t bytes,DollyDlySetInfo *out) {
    const char *cursor=text,*end;
    uint64_t parts,sector,first,sectors,per_part;
    if(!text || !out)return DOLLY_BLOCK_EINVAL;
    end=text+bytes;
    if(!manifest_literal(&cursor,end,DOLLY_DLY_SET_MAGIC "Format: IFF DOLY\n") ||
       !manifest_number(&cursor,end,"Parts: ",&parts) ||
       !manifest_number(&cursor,end,"Sector bytes: ",&sector) ||
       !manifest_number(&cursor,end,"First LBA: ",&first) ||
       !manifest_number(&cursor,end,"Sectors: ",&sectors) ||
       !manifest_number(&cursor,end,"Sectors per part: ",&per_part) ||
       cursor!=end || sector<512u || sector>32768u || (sector&(sector-1u)) ||
       !sectors || sectors>UINT64_MAX/sector || first>UINT64_MAX-(sectors-1u) ||
       !per_part || per_part>INT32_MAX/sector ||
       !parts || parts>UINT32_MAX || parts!=(sectors-1u)/per_part+1u)
        return DOLLY_BLOCK_EINVAL;
    *out=(DollyDlySetInfo){first,sectors,(uint32_t)sector,(uint32_t)parts,(uint32_t)per_part,0,0};
    return DOLLY_BLOCK_OK;
}

int dolly_dly_set_matches(const DollyDlySetInfo *set,uint32_t index,
                          const DollyDlyImageInfo *part) {
    uint64_t start=(uint64_t)index*set->sectors_per_part;
    uint64_t remaining;
    if(index>=set->parts || start>=set->sectors)return 0;
    remaining=set->sectors-start;
    if(remaining>set->sectors_per_part)remaining=set->sectors_per_part;
    return part->sector_bytes==set->sector_bytes && part->sectors==remaining &&
           part->first_lba==set->first_lba+start;
}
