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
                p->dos_type,DOLLY_INV_RDB_PART,DOLLY_INV_PRESENT);
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
    EMIT_NUMBER("max_transfer","MaxTransfer (filesystem)","bytes",
                p->max_transfer_bytes,DOLLY_INV_RDB_PART,
                p->max_transfer_present?DOLLY_INV_PRESENT:DOLLY_INV_UNAVAILABLE);
#undef EMIT_NUMBER
    return 0;
}
