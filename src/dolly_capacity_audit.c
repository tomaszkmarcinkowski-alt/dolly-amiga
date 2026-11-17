#include "dolly_capacity_audit.h"
#include <stdint.h>
#include <string.h>
static int checked_mul(uint64_t a,uint64_t b,uint64_t *out){
    if(b && a>UINT64_MAX/b)return 0;
    *out=a*b;return 1;
}
static int inside(uint64_t start,uint64_t length,
                  uint64_t base,uint64_t capacity){
    return start>=base && length<=capacity &&
           start-base<=capacity-length;
}
DollyCapacityAudit dolly_capacity_audit(const DollyRdb *r,
                                         uint64_t base,uint64_t bytes){
    DollyCapacityAudit out;
    uint64_t units=0,header=0;
    unsigned i;
    memset(&out,0,sizeof(out));
    out.observed_container_bytes=bytes;
    if(!r||!r->block_bytes||!bytes||bytes>UINT64_MAX-base||
       r->base_offset!=base||r->partition_count>DOLLY_RDB_MAX_PARTITIONS){
        out.flags=DOLLY_CAPACITY_INVALID;return out;
    }
    if(!r->cylinders||!r->heads||!r->sectors){
        out.flags|=DOLLY_CAPACITY_GEOMETRY_UNKNOWN;
    }else if(!checked_mul(r->cylinders,r->heads,&units)||
             !checked_mul(units,r->sectors,&units)||
             !checked_mul(units,r->block_bytes,&units)){
        out.flags|=DOLLY_CAPACITY_OVERFLOW;
    }else{
        out.declared_geometry_bytes=units;
        if(units<bytes){
            out.flags|=DOLLY_CAPACITY_GEOMETRY_SMALLER;
            out.extra_bytes=bytes-units;
        }else if(units>bytes){
            out.flags|=DOLLY_CAPACITY_GEOMETRY_LARGER;
            out.shortfall_bytes=units-bytes;
        }
    }
    if(!checked_mul(r->header_block,r->block_bytes,&header)||
       header>UINT64_MAX-base||
       !inside(base+header,r->block_bytes,base,bytes))
        out.flags|=DOLLY_CAPACITY_META_OUTSIDE;
    for(i=0;i<r->partition_count;i++){
        const DollyPartition *p=&r->partitions[i];
        uint64_t pointer=0;
        if(!checked_mul(p->descriptor_block,r->block_bytes,&pointer)||
           pointer>UINT64_MAX-base||
           !inside(base+pointer,r->block_bytes,base,bytes))
            out.flags|=DOLLY_CAPACITY_META_OUTSIDE;
        if(!inside(p->start_bytes,p->length_bytes,base,bytes)){
            if(!(out.flags&DOLLY_CAPACITY_PART_OUTSIDE))
                out.first_outside_partition=i;
            out.flags|=DOLLY_CAPACITY_PART_OUTSIDE;
        }
        if(out.declared_geometry_bytes &&
           !inside(p->start_bytes,p->length_bytes,base,
                   out.declared_geometry_bytes))
            out.flags|=DOLLY_CAPACITY_PART_PAST_GEOM;
    }
    return out;
}
