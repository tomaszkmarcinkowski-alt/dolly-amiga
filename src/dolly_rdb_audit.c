#include "dolly_rdb_audit.h"
#include <stdint.h>
#include <string.h>
static int contains(uint64_t base, uint64_t len, uint64_t start, uint64_t size) {
    return start>=base && size<=len && start-base<=len-size;
}
static int overlap(uint64_t a, uint64_t n, uint64_t b, uint64_t m) {
    /* Call only after ensuring both ranges fit within a common container. */
    return a<b+m && b<a+n;
}
DollyRdbAudit dolly_rdb_audit(const DollyRdb *r,
                             uint64_t base, uint64_t capacity) {
    DollyRdbAudit result;
    uint64_t header, pointers[DOLLY_RDB_MAX_PARTITIONS];
    unsigned i,j;
    memset(&result,0,sizeof(result));
    if(!r || !r->block_bytes || r->partition_count>DOLLY_RDB_MAX_PARTITIONS ||
       r->base_offset!=base || capacity>UINT64_MAX-base) {
        result.flags=DOLLY_RDB_AUDIT_BAD_ARGUMENT;
        return result;
    }
    if(r->header_block>(UINT64_MAX-base)/r->block_bytes) {
        result.flags=DOLLY_RDB_AUDIT_BAD_ARGUMENT;
        return result;
    }
    header=base+(uint64_t)r->header_block*r->block_bytes;
    if(!contains(base,capacity,header,r->block_bytes))
        result.flags|=DOLLY_RDB_AUDIT_OUTSIDE_CONTAINER;
    for(i=0;i<r->partition_count;i++){
        const DollyPartition *p=&r->partitions[i];
        if(p->descriptor_block>(UINT64_MAX-base)/r->block_bytes) {
            result.flags|=DOLLY_RDB_AUDIT_OUTSIDE_CONTAINER;
            return result;
        }
        pointers[i]=base+(uint64_t)p->descriptor_block*r->block_bytes;
        if(!contains(base,capacity,pointers[i],r->block_bytes) ||
           !contains(base,capacity,p->start_bytes,p->length_bytes))
            result.flags|=DOLLY_RDB_AUDIT_OUTSIDE_CONTAINER;
    }
    if(result.flags & DOLLY_RDB_AUDIT_OUTSIDE_CONTAINER)return result;
    for(i=0;i<r->partition_count;i++){
        const DollyPartition *p=&r->partitions[i];
        if(overlap(p->start_bytes,p->length_bytes,header,r->block_bytes)){
            result.flags|=DOLLY_RDB_AUDIT_HEADER_COLLISION;
            result.first_partition=i;
        }
        for(j=0;j<r->partition_count;j++){
            if(overlap(p->start_bytes,p->length_bytes,pointers[j],r->block_bytes)){
                result.flags|=DOLLY_RDB_AUDIT_PART_COLLISION;
                result.first_partition=i;
                result.second_partition=j;
            }
            if(j<i && overlap(p->start_bytes,p->length_bytes,
                       r->partitions[j].start_bytes,r->partitions[j].length_bytes)){
                result.flags|=DOLLY_RDB_AUDIT_OVERLAPPING_PARTS;
                result.first_partition=j;
                result.second_partition=i;
            }
        }
    }
    return result;
}
