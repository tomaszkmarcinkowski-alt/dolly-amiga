#include "dolly_rdb_audit.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void){
 DollyRdb r;DollyRdbAudit result;
 memset(&r,0,sizeof(r));r.block_bytes=512;r.base_offset=512*100;r.header_block=1;
 r.partition_count=2;
 r.partitions[0].descriptor_block=2;
 r.partitions[0].start_bytes=512*130;r.partitions[0].length_bytes=512*20;
 r.partitions[1].descriptor_block=3;
 r.partitions[1].start_bytes=512*150;r.partitions[1].length_bytes=512*30;
 result=dolly_rdb_audit(&r,r.base_offset,512*200);
 assert(result.flags==0);
 r.partitions[1].start_bytes=512*140;
 result=dolly_rdb_audit(&r,r.base_offset,512*200);
 assert(result.flags&DOLLY_RDB_AUDIT_OVERLAPPING_PARTS);
 r.partitions[1].start_bytes=512*150;
 r.partitions[0].start_bytes=512*101;
 result=dolly_rdb_audit(&r,r.base_offset,512*200);
 assert(result.flags&DOLLY_RDB_AUDIT_HEADER_COLLISION);
 assert(result.flags&DOLLY_RDB_AUDIT_PART_COLLISION);
 r.partitions[0].start_bytes=512*130;
 r.partitions[1].start_bytes=512*500;
 result=dolly_rdb_audit(&r,r.base_offset,512*200);
 assert(result.flags&DOLLY_RDB_AUDIT_OUTSIDE_CONTAINER);
 result=dolly_rdb_audit(&r,r.base_offset,UINT64_MAX);
 assert(result.flags&DOLLY_RDB_AUDIT_BAD_ARGUMENT);
 puts("RDB collision audit tests passed");return 0;
}
