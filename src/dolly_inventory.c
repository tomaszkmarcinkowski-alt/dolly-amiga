#include "dolly_inventory.h"
#include <stdint.h>
static int numeric(DollyInventoryEmit emit,void *user,const char *key,
                   const char *label,const char *unit,uint64_t value,
                   DollyInventoryOrigin origin,DollyInventoryAvailability available){
    DollyInventoryField f;
    f.key=key;f.label=label;f.unit=unit;f.kind=DOLLY_INV_U64;
    f.availability=available;f.origin=origin;f.number=value;f.text=0;
    return emit(&f,user);
}
static int string(DollyInventoryEmit emit,void *user,const char *key,
                  const char *label,const char *value){
    DollyInventoryField f;
    f.key=key;f.label=label;f.unit="";f.kind=DOLLY_INV_TEXT;
    f.availability=DOLLY_INV_PRESENT;f.origin=DOLLY_INV_RDB_PART;
    f.number=0;f.text=value;
    return emit(&f,user);
}
int dolly_inventory_partition(const DollyPartition *p,
                              DollyInventoryEmit emit,void *user){
    int r;
    if(!p||!emit)return -1;
#define EMIT_NUMBER(k,l,u,v,o,a) do { \
    r=numeric(emit,user,k,l,u,(uint64_t)(v),o,a); if(r)return r; \
} while(0)
    r=string(emit,user,"name","Partition name",p->name);if(r)return r;
    EMIT_NUMBER("descriptor_block","PART descriptor block","block",
                p->descriptor_block,DOLLY_INV_RDB_PART,DOLLY_INV_PRESENT);
    EMIT_NUMBER("dos_type","DosType","hex32",
                p->dos_type,DOLLY_INV_RDB_PART,
                p->dos_type_present?DOLLY_INV_PRESENT:DOLLY_INV_UNAVAILABLE);
    EMIT_NUMBER("low_cylinder","Low cylinder","cylinder",
                p->low_cylinder,DOLLY_INV_RDB_PART,DOLLY_INV_PRESENT);
    EMIT_NUMBER("high_cylinder","High cylinder","cylinder",
                p->high_cylinder,DOLLY_INV_RDB_PART,DOLLY_INV_PRESENT);
    EMIT_NUMBER("start_bytes","Start offset","bytes",
                p->start_bytes,DOLLY_INV_DERIVED,DOLLY_INV_PRESENT);
    EMIT_NUMBER("length_bytes","Length","bytes",
                p->length_bytes,DOLLY_INV_DERIVED,DOLLY_INV_PRESENT);
    /* End-exclusive calculation avoids overflow. */
    EMIT_NUMBER("end_exclusive_bytes","End offset (exclusive)","bytes",
                p->length_bytes<=UINT64_MAX-p->start_bytes
                  ? p->start_bytes+p->length_bytes : 0,
                DOLLY_INV_DERIVED,
                p->length_bytes<=UINT64_MAX-p->start_bytes
                  ? DOLLY_INV_PRESENT:DOLLY_INV_UNAVAILABLE);
    EMIT_NUMBER("buffers","Initial DOS buffers","count",
                p->buffer_count,DOLLY_INV_RDB_PART,
                p->buffer_count_present?DOLLY_INV_PRESENT:DOLLY_INV_UNAVAILABLE);
    EMIT_NUMBER("buffer_mem_type","DOS buffer memory type","hex32",
                p->buffer_memory_type,DOLLY_INV_RDB_PART,
                p->buffer_memory_type_present?DOLLY_INV_PRESENT:DOLLY_INV_UNAVAILABLE);
    EMIT_NUMBER("dma_mask","Memory address mask","hex32",
                p->dma_mask,DOLLY_INV_RDB_PART,
                p->dma_mask_present?DOLLY_INV_PRESENT:DOLLY_INV_UNAVAILABLE);
    EMIT_NUMBER("max_transfer","MaxTransfer (filesystem)","bytes",
                p->max_transfer_bytes,DOLLY_INV_RDB_PART,
                p->max_transfer_present?DOLLY_INV_PRESENT:DOLLY_INV_UNAVAILABLE);
#undef EMIT_NUMBER
    return 0;
}

/* Read-only stream for a primary MBR entry. No additional disk accesses. */
int dolly_inventory_mbr_partition(const DollyMbrPartition *p,
                                 DollyInventoryEmit emit,void *user){
    int r;
    if(!p||!emit)return -1;
#define MBR_FIELD(k,l,u,v) do { \
    r=numeric(emit,user,k,l,u,(uint64_t)(v),DOLLY_INV_MBR,DOLLY_INV_PRESENT); \
    if(r)return r; \
} while(0)
    MBR_FIELD("mbr_type","MBR partition type","hex32",p->type);
    MBR_FIELD("mbr_bootable","MBR active flag","boolean",p->bootable);
    MBR_FIELD("mbr_first_lba","First LBA","sector",p->first_lba);
    MBR_FIELD("mbr_sectors","Sector count","sectors",p->sector_count);
    MBR_FIELD("mbr_start","Byte offset","bytes",p->start_bytes);
    MBR_FIELD("mbr_length","Length","bytes",p->length_bytes);
    MBR_FIELD("mbr_contains_rdb","Nested RDB recognized","boolean",p->contains_rdb);
#undef MBR_FIELD
    return 0;
}

int dolly_inventory_rdb_header(const DollyRdb *r,DollyInventoryEmit emit,void *user){
    int x;
    if(!r||!emit)return -1;
#define RDB_FIELD(k,l,u,v,o) do { \
    x=numeric(emit,user,k,l,u,(uint64_t)(v),o,DOLLY_INV_PRESENT); \
    if(x)return x; \
} while(0)
    RDB_FIELD("rdb_base","RDB base","bytes",r->base_offset,DOLLY_INV_DERIVED);
    RDB_FIELD("rdb_block_bytes","RDB block size","bytes",r->block_bytes,DOLLY_INV_RDB_HEADER);
    RDB_FIELD("rdb_header_block","RDSK position","block",r->header_block,DOLLY_INV_RDB_HEADER);
    RDB_FIELD("rdb_cylinders","RDB cylinders","cylinders",r->cylinders,DOLLY_INV_RDB_HEADER);
    RDB_FIELD("rdb_heads","RDB heads","heads",r->heads,DOLLY_INV_RDB_HEADER);
    RDB_FIELD("rdb_sectors","RDB sectors/track","sectors",r->sectors,DOLLY_INV_RDB_HEADER);
    RDB_FIELD("rdb_partition_count","Recognized PART count","partitions",r->partition_count,DOLLY_INV_DERIVED);
#undef RDB_FIELD
    return 0;
}
const char *dolly_inventory_availability_name(DollyInventoryAvailability s){
    switch(s){
    case DOLLY_INV_PRESENT:return "present";
    case DOLLY_INV_UNAVAILABLE:return "not present";
    case DOLLY_INV_NOT_QUERIED:return "not queried";
    case DOLLY_INV_UNSUPPORTED:return "unsupported";
    case DOLLY_INV_INCONSISTENT:return "inconsistent";
    default:return "invalid status";
    }
}
