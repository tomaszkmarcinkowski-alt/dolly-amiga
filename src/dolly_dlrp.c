#include "dolly_dlrp.h"
#include "dolly_checksum.h"
#include "dolly_restore_transfer.h"
#include <string.h>

void dolly_dlrp_u32(unsigned char *out,uint32_t value) {
    out[0]=(unsigned char)(value>>24);out[1]=(unsigned char)(value>>16);
    out[2]=(unsigned char)(value>>8);out[3]=(unsigned char)value;
}
void dolly_dlrp_u64(unsigned char *out,uint64_t value) {
    dolly_dlrp_u32(out,(uint32_t)(value>>32));
    dolly_dlrp_u32(out+4,(uint32_t)value);
}
int dolly_dlrp_form(DollyDlySink sink,void *ctx,uint32_t body_bytes) {
    unsigned char header[12]="FORM";
    if(body_bytes>UINT32_MAX-4u || body_bytes%2u)return 0;
    dolly_dlrp_u32(header+4,body_bytes+4u);
    memcpy(header+8,"DLRP",4);
    return sink(ctx,header,sizeof(header));
}
int dolly_dlrp_chunk(DollyDlySink sink,void *ctx,const char id[4],
                     const void *data,uint32_t bytes) {
    unsigned char header[8],padding=0;
    memcpy(header,id,4);
    dolly_dlrp_u32(header+4,bytes);
    return sink(ctx,header,sizeof(header)) && (!bytes || sink(ctx,data,bytes)) &&
           (!(bytes&1u) || sink(ctx,&padding,1));
}

static uint32_t be32(const unsigned char *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}

static uint64_t be64(const unsigned char *p) {
    return (uint64_t)be32(p)<<32 | be32(p+4);
}

typedef struct DlrpInput {
    DollyDlrpRead read;
    void *context;
    uint32_t left;
    DollyChecksum checksum;
} DlrpInput;

static int take(DlrpInput *in,void *data,uint32_t bytes) {
    if(bytes>in->left || (bytes && !in->read(in->context,data,bytes)))return 0;
    in->left-=bytes;
    return dolly_checksum_update(&in->checksum,data,bytes)==0;
}

static int fixed_chunk(DlrpInput *in,const char *id,void *data,uint32_t bytes) {
    unsigned char header[8];
    return take(in,header,sizeof(header)) && !memcmp(header,id,4) &&
           be32(header+4)==bytes && take(in,data,bytes);
}

static int string_chunk(DlrpInput *in,const char *id,char *data,uint32_t capacity) {
    unsigned char header[8],padding;
    uint32_t size;
    if(!take(in,header,sizeof(header)) || memcmp(header,id,4))return 0;
    size=be32(header+4);
    if(size>=capacity || !take(in,data,size) || memchr(data,0,size))return 0;
    data[size]='\0';
    return !(size&1u) || take(in,&padding,1);
}

DollyDlrpStatus dolly_dlrp_read_image(DollyDlrpRead read,void *context,uint32_t length,
                                     DollyDlrpImageResume *r) {
    unsigned char header[12],head[108],geometry[36],output[24],event[20];
    uint32_t kind,batch,partition,format,codec,crc;
    DlrpInput in={.read=read,.context=context,.left=length};
    memset(r,0,sizeof(*r));
    dolly_checksum_init(&in.checksum);
    if(!take(&in,header,sizeof(header)) || memcmp(header,"FORM",4) ||
       memcmp(header+8,"DLRP",4) || be32(header+4)!=length-8u ||
       !take(&in,header,8) || memcmp(header,"HEAD",4) ||
       be32(header+4)<8 || !take(&in,head,8))return DOLLY_DLRP_INVALID;
    if(be32(head)!=1 || be32(head+4)!=2)return DOLLY_DLRP_UNSUPPORTED;
    if(be32(header+4)!=sizeof(head) || !take(&in,head+8,sizeof(head)-8) ||
       !fixed_chunk(&in,"GEOM",geometry,sizeof(geometry)) ||
       !string_chunk(&in,"SRCE",r->source.path,sizeof(r->source.path)) ||
       !string_chunk(&in,"DRVR",r->driver,sizeof(r->driver)) ||
       !string_chunk(&in,"NAME",r->device_name,sizeof(r->device_name)) ||
       !fixed_chunk(&in,"OUTP",output,sizeof(output)) ||
       !string_chunk(&in,"DEST",r->target.path,sizeof(r->target.path)) ||
       !string_chunk(&in,"PART",r->partial,sizeof(r->partial)))return DOLLY_DLRP_INVALID;
    while(in.left>12) {
        if(!fixed_chunk(&in,"EVNT",event,sizeof(event)))return DOLLY_DLRP_INVALID;
    }
    crc=dolly_checksum_final(&in.checksum);
    if(!fixed_chunk(&in,"CR32",header,4) || in.left || be32(header)!=crc)
        return DOLLY_DLRP_INVALID;

    format=be32(output);
    if(format!=DOLLY_IMAGE_RAW && format!=DOLLY_IMAGE_HDF &&
       format!=DOLLY_IMAGE_IFF_DOLY && format!=DOLLY_IMAGE_IFF_FAST &&
       format!=DOLLY_IMAGE_IFF_COMPACT)return DOLLY_DLRP_UNSUPPORTED;
    kind=be32(head+8);
    partition=be32(head+104);
    r->unit=be32(head+12);
    r->sector_bytes=be32(head+16);
    batch=be32(head+20);
    r->first_lba=be64(head+24);
    r->sectors=be64(head+32);
    r->completed=be64(head+40);
    r->capacity=be64(head+48);
    r->anchors=be32(head+80);
    r->first_crc=be32(head+84);
    r->last_crc=be32(head+88);
    r->capacity_source=be32(geometry);
    r->cylinders=be32(geometry+4);
    r->heads=be32(geometry+8);
    r->sectors_per_track=be32(geometry+12);
    r->source.partition_start_bytes=be64(geometry+20);
    r->source.partition_length_bytes=be64(geometry+28);
    r->output_bytes=be32(output+4);
    r->batch_bytes=batch;
    r->dly_file_bytes=be32(output+12);
    codec=format==DOLLY_IMAGE_IFF_FAST?DOLLY_DLY_PACKBITS:
          format==DOLLY_IMAGE_IFF_COMPACT?DOLLY_DLY_LZSS:DOLLY_DLY_RAW;
    r->codec=(DollyDlyCodec)codec;
    if((kind!=DOLLY_ENDPOINT_UNIT && kind!=DOLLY_ENDPOINT_PARTITION &&
        kind!=DOLLY_ENDPOINT_LOGICAL) || partition>UINT8_MAX ||
       !*r->source.path || !*r->driver || !*r->target.path || !*r->partial ||
       r->sector_bytes<512 || r->sector_bytes>32768 ||
       (r->sector_bytes&(r->sector_bytes-1)) || batch<r->sector_bytes ||
       batch>32768 || batch%r->sector_bytes || !r->anchors || (r->anchors&~3u) ||
       !r->sectors || r->sectors>(uint32_t)INT32_MAX/r->sector_bytes ||
       r->first_lba>r->capacity || r->sectors>r->capacity-r->first_lba ||
       r->capacity>UINT64_MAX/r->sector_bytes || r->completed>r->sectors ||
       codec!=be32(output+8))
        return DOLLY_DLRP_INVALID;
    if(format==DOLLY_IMAGE_RAW || format==DOLLY_IMAGE_HDF) {
        if(r->output_bytes!=r->completed*r->sector_bytes ||
           r->dly_file_bytes || be64(output+16))return DOLLY_DLRP_INVALID;
    } else if(r->output_bytes<DOLLY_DLY_GEOM_END ||
              r->output_bytes>r->dly_file_bytes || r->dly_file_bytes>INT32_MAX ||
              be64(output+16)!=r->completed ||
              (r->completed!=r->sectors && r->completed*r->sector_bytes%batch)) {
        return DOLLY_DLRP_INVALID;
    }
    if(kind==DOLLY_ENDPOINT_UNIT && (r->first_lba || r->sectors!=r->capacity))
        return DOLLY_DLRP_INVALID;
    if(kind==DOLLY_ENDPOINT_PARTITION &&
       (r->source.partition_start_bytes!=r->first_lba*r->sector_bytes ||
        r->source.partition_length_bytes!=r->sectors*r->sector_bytes))
        return DOLLY_DLRP_INVALID;
    r->source.kind=(DollyEndpointKind)kind;
    r->source.partition=(uint8_t)partition;
    r->target.kind=DOLLY_ENDPOINT_FILE;
    r->target.image_format=(DollyImageFormat)format;
    return DOLLY_DLRP_READY;
}

static int transfer_extent(DollyDlrpTransferExtent *e,const unsigned char data[76],
                            uint32_t sector,int file) {
    uint32_t kind=be32(data),partition=be32(data+8),format=be32(data+48);
    e->unit=be32(data+4);
    e->start=be64(data+16);
    e->length=be64(data+24);
    e->capacity=be64(data+32);
    e->first_crc=be32(data+52);
    e->last_crc=be32(data+56);
    e->capacity_source=be32(data+60);
    e->cylinders=be32(data+64);
    e->heads=be32(data+68);
    e->sectors_per_track=be32(data+72);
    if(!*e->endpoint.path || !e->length || e->start>e->capacity ||
       e->length>e->capacity-e->start || e->start%sector || e->length%sector ||
       e->capacity%sector || partition>UINT8_MAX)return 0;
    if(file) {
        if(kind!=DOLLY_ENDPOINT_FILE || e->start || e->length!=e->capacity ||
           *e->driver || *e->name ||
           (format!=DOLLY_IMAGE_RAW && format!=DOLLY_IMAGE_HDF &&
            format!=DOLLY_IMAGE_IFF_DOLY))return 0;
    } else if(!*e->driver ||
              (kind!=DOLLY_ENDPOINT_UNIT && kind!=DOLLY_ENDPOINT_PARTITION) ||
              (kind==DOLLY_ENDPOINT_UNIT && (e->start || e->length!=e->capacity)))return 0;
    e->endpoint.kind=(DollyEndpointKind)kind;
    e->endpoint.partition=(uint8_t)partition;
    if(file)e->endpoint.image_format=(DollyImageFormat)format;
    if(kind==DOLLY_ENDPOINT_PARTITION) {
        e->endpoint.partition_start_bytes=e->start;
        e->endpoint.partition_length_bytes=e->length;
    }
    return 1;
}

DollyDlrpStatus dolly_dlrp_read_transfer(DollyDlrpRead read,void *context,uint32_t length,
                                        DollyDlrpTransferResume *r) {
    unsigned char header[12],head[56],src[76],dst[76];
    uint32_t operation,state,read_status,write_status,crc;
    uint64_t batch;
    DlrpInput in={.read=read,.context=context,.left=length};
    memset(r,0,sizeof(*r));
    dolly_checksum_init(&in.checksum);
    if(!take(&in,header,sizeof(header)) || memcmp(header,"FORM",4) ||
       memcmp(header+8,"DLRP",4) || be32(header+4)!=length-8u ||
       !take(&in,header,8) || memcmp(header,"HEAD",4) ||
       be32(header+4)<8 || !take(&in,head,8))return DOLLY_DLRP_INVALID;
    operation=be32(head+4);
    if(be32(head)!=2 || (operation!=3 && operation!=4))return DOLLY_DLRP_UNSUPPORTED;
    if(be32(header+4)!=sizeof(head) || !take(&in,head+8,sizeof(head)-8) ||
       !fixed_chunk(&in,"SRC2",src,sizeof(src)) || !fixed_chunk(&in,"DST2",dst,sizeof(dst)) ||
       !string_chunk(&in,"SRCE",r->source.endpoint.path,sizeof(r->source.endpoint.path)) ||
       !string_chunk(&in,"DEST",r->target.endpoint.path,sizeof(r->target.endpoint.path)) ||
       !string_chunk(&in,"DRVR",r->source.driver,sizeof(r->source.driver)) ||
       !string_chunk(&in,"TDRV",r->target.driver,sizeof(r->target.driver)) ||
       !string_chunk(&in,"NAME",r->source.name,sizeof(r->source.name)) ||
       !string_chunk(&in,"TNAM",r->target.name,sizeof(r->target.name)))return DOLLY_DLRP_INVALID;
    crc=dolly_checksum_final(&in.checksum);
    if(!fixed_chunk(&in,"CR32",header,4) || in.left || be32(header)!=crc)
        return DOLLY_DLRP_INVALID;
    r->restoring=operation==4;
    r->sector_bytes=be32(head+8);
    r->batch_bytes=be32(head+12);
    r->verify=be32(head+16)==1;
    state=be32(head+20);
    r->committed=be64(head+24);
    r->uncertain=be64(head+32);
    r->bytes=be64(head+40);
    read_status=be32(head+48);
    write_status=be32(head+52);
    if(r->sector_bytes<512 || r->sector_bytes>32768 ||
       (r->sector_bytes&(r->sector_bytes-1)) || r->batch_bytes<r->sector_bytes ||
       r->batch_bytes>32768 || r->batch_bytes%r->sector_bytes || be32(head+16)>1 ||
       r->committed>=r->bytes || r->bytes%r->sector_bytes || r->committed%r->batch_bytes ||
       (state!=DOLLY_RESTORE_READ_FAILED && state!=DOLLY_RESTORE_WRITE_FAILED) ||
       read_status>DOLLY_BLOCK_EDEVICE || write_status>DOLLY_WRITE_COMPLETE ||
       !transfer_extent(&r->source,src,r->sector_bytes,r->restoring) ||
       !transfer_extent(&r->target,dst,r->sector_bytes,0) ||
       (!r->restoring && r->source.length!=r->target.length) ||
       r->bytes!=(r->source.length<r->target.length?r->source.length:r->target.length))
        return DOLLY_DLRP_INVALID;
    batch=r->bytes-r->committed;
    if(batch>r->batch_bytes)batch=r->batch_bytes;
    if(state==DOLLY_RESTORE_WRITE_FAILED &&
       (read_status!=DOLLY_BLOCK_OK ||
        (write_status!=DOLLY_WRITE_REJECTED && (write_status<DOLLY_WRITE_FAILED ||
          write_status>DOLLY_WRITE_MISMATCH))))return DOLLY_DLRP_INVALID;
    if(r->uncertain!=(state==DOLLY_RESTORE_WRITE_FAILED && write_status!=DOLLY_WRITE_REJECTED?batch:0))
        return DOLLY_DLRP_INVALID;
    return DOLLY_DLRP_READY;
}
