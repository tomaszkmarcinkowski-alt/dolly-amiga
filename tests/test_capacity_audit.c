#include "dolly_capacity_audit.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static DollyRdb r;
int main(void){
    DollyCapacityAudit a;
    memset(&r,0,sizeof(r));
    r.block_bytes=512;r.cylinders=1;r.heads=4;r.sectors=32;
    r.header_block=0;r.partition_count=1;
    r.partitions[0].descriptor_block=1;
    r.partitions[0].start_bytes=4096;
    r.partitions[0].length_bytes=65536u-4096u;
    a=dolly_capacity_audit(&r,0,65536u);
    assert(a.flags==0 && a.extra_bytes==0);
    a=dolly_capacity_audit(&r,0,100000u);
    assert(a.flags==DOLLY_CAPACITY_GEOMETRY_SMALLER);
    assert(a.extra_bytes==34464u); /* not automatically safe to write */
    a=dolly_capacity_audit(&r,0,62000u);
    assert(a.flags&DOLLY_CAPACITY_GEOMETRY_LARGER);
    assert(a.flags&DOLLY_CAPACITY_PART_OUTSIDE);
    assert(a.first_outside_partition==0);
    assert(a.shortfall_bytes==3536u);
    r.partitions[0].length_bytes=55000u;
    a=dolly_capacity_audit(&r,0,62000u);
    assert(a.flags==DOLLY_CAPACITY_GEOMETRY_LARGER);
    r.partitions[0].length_bytes=65536u-4096u;
    r.base_offset=4096;
    r.partitions[0].start_bytes+=4096;
    a=dolly_capacity_audit(&r,4096,65536u);
    assert(a.flags==0); /* nested MBR container: relative geometry */
    a=dolly_capacity_audit(&r,4096,62000u);
    assert(a.flags&DOLLY_CAPACITY_PART_OUTSIDE);
    a=dolly_capacity_audit(&r,0,65536u);
    assert(a.flags==DOLLY_CAPACITY_INVALID);
    r.base_offset=0;r.cylinders=0;
    a=dolly_capacity_audit(&r,0,65536u);
    assert(a.flags&DOLLY_CAPACITY_GEOMETRY_UNKNOWN);
    assert(!(a.flags&DOLLY_CAPACITY_GEOMETRY_LARGER));
    r.cylinders=1;r.partitions[0].start_bytes=4096;
    r.partitions[0].descriptor_block=500;
    a=dolly_capacity_audit(&r,0,65536u);
    assert(a.flags&DOLLY_CAPACITY_META_OUTSIDE);
    r.partitions[0].descriptor_block=1;
    r.partitions[0].start_bytes=65536;
    r.partitions[0].length_bytes=512;
    a=dolly_capacity_audit(&r,0,100000u);
    assert(a.flags&DOLLY_CAPACITY_PART_PAST_GEOM);
    assert(!(a.flags&DOLLY_CAPACITY_PART_OUTSIDE));
    r.cylinders=UINT32_MAX;r.heads=UINT32_MAX;r.sectors=UINT32_MAX;
    a=dolly_capacity_audit(&r,0,100000u);
    assert(a.flags&DOLLY_CAPACITY_OVERFLOW);
    assert(a.declared_geometry_bytes==0);
    puts("RDB independent-media capacity audit passed");
    return 0;
}
