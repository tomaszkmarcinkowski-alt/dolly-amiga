#include "dolly_partition_map.h"
#include <stdint.h>
#include <string.h>
static uint32_t le32(const unsigned char *p) {
 return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}
static int range_inside(uint64_t start,uint64_t size,uint64_t base,uint64_t len) {
 return start>=base && size<=len && start-base<=len-size;
}
static int overlaps(uint64_t a,uint64_t n,uint64_t b,uint64_t m) {
 return a<b+m && b<a+n; /* All ranges prevalidated within source. */
}
static int pow2_block(uint32_t n) {return n>=512 && n<=32768 && !(n&(n-1));}
static DollyMapStatus check_inner(const DollyMbrPartition *p){
 unsigned i,j;const DollyRdb *r=&p->nested_rdb;uint64_t header;
 if(!p->contains_rdb)return DOLLY_MAP_OK;
 header=p->start_bytes+(uint64_t)r->header_block*r->block_bytes;
 if(!range_inside(header,r->block_bytes,p->start_bytes,p->length_bytes))
  return DOLLY_MAP_OUT_OF_RANGE;
 for(i=0;i<r->partition_count;i++){
  const DollyPartition *child=&r->partitions[i];
  if(!range_inside(child->start_bytes,child->length_bytes,p->start_bytes,p->length_bytes))
   return DOLLY_MAP_OUT_OF_RANGE;
  if(overlaps(child->start_bytes,child->length_bytes,header,r->block_bytes))
   return DOLLY_MAP_OVERLAP;
  for(j=0;j<r->partition_count;j++){
   const DollyPartition *descriptor=&r->partitions[j];
   uint64_t dp=p->start_bytes+(uint64_t)descriptor->descriptor_block*r->block_bytes;
   if(!range_inside(dp,r->block_bytes,p->start_bytes,p->length_bytes) ||
      overlaps(child->start_bytes,child->length_bytes,dp,r->block_bytes))
    return DOLLY_MAP_OVERLAP;
  }
  for(j=0;j<i;j++){
   const DollyPartition *other=&r->partitions[j];
   if(overlaps(child->start_bytes,child->length_bytes,
               other->start_bytes,other->length_bytes))return DOLLY_MAP_OVERLAP;
  }
 }
 return DOLLY_MAP_OK;
}
DollyMapStatus dolly_partition_map_scan(DollyBlockSource *source,
 uint32_t block_bytes,DollyPartitionMap *out){
 unsigned char mbr[512];unsigned i,j;DollyBlockReadResult rr;DollyRdbStatus rs;
 if(!source||!out||!pow2_block(block_bytes))return DOLLY_MAP_BAD_ARGUMENT;
 memset(out,0,sizeof(*out));
 if(dolly_block_size(source)<512)return DOLLY_MAP_NO_TABLE;
 rr=dolly_block_read(source,0,mbr,sizeof(mbr));
 if(rr.status!=DOLLY_BLOCK_OK||rr.bytes_read!=sizeof(mbr))return DOLLY_MAP_READ_ERROR;
 if(mbr[510]!=0x55||mbr[511]!=0xaa){
  rs=dolly_rdb_scan(source,0,block_bytes,16,&out->whole_disk_rdb);
  if(rs==DOLLY_RDB_OK){out->has_whole_disk_rdb=1;return DOLLY_MAP_OK;}
  return rs==DOLLY_RDB_NOT_FOUND?DOLLY_MAP_NO_TABLE:DOLLY_MAP_NESTED_RDB_ERROR;
 }
 out->has_mbr=1;
 for(i=0;i<4;i++){
  const unsigned char *p=mbr+446+i*16;
  uint32_t lba=le32(p+8),count=le32(p+12);
  DollyMbrPartition *d;
  if(p[4]==0 && lba==0 && count==0)continue;
  if(!count||!lba||(p[0]!=0&&p[0]!=0x80))return DOLLY_MAP_UNSUPPORTED;
  if(p[4]==0xee||p[4]==0x05||p[4]==0x0f||p[4]==0x85)
   return DOLLY_MAP_UNSUPPORTED;
  d=&out->primary[out->partition_count++];
  d->bootable=p[0]==0x80;d->type=p[4];d->first_lba=lba;d->sector_count=count;
  d->start_bytes=(uint64_t)lba*block_bytes;
  d->length_bytes=(uint64_t)count*block_bytes;
  if(!range_inside(d->start_bytes,d->length_bytes,0,dolly_block_size(source)))
   return DOLLY_MAP_OUT_OF_RANGE;
  for(j=0;j+1<out->partition_count;j++){
   const DollyMbrPartition *other=&out->primary[j];
   if(overlaps(d->start_bytes,d->length_bytes,
               other->start_bytes,other->length_bytes))return DOLLY_MAP_OVERLAP;
  }
 }
 for(i=0;i<out->partition_count;i++){
  DollyMbrPartition *d=&out->primary[i];
  if(d->type!=0x76)continue;
  if(d->start_bytes%block_bytes||d->length_bytes%block_bytes)
   return DOLLY_MAP_UNSUPPORTED;
  rs=dolly_rdb_scan(source,d->start_bytes,block_bytes,16,&d->nested_rdb);
  if(rs==DOLLY_RDB_NOT_FOUND)continue;
  if(rs!=DOLLY_RDB_OK)return DOLLY_MAP_NESTED_RDB_ERROR;
  d->contains_rdb=1;
  {DollyMapStatus s=check_inner(d);if(s!=DOLLY_MAP_OK)return s;}
 }
 /* Advanced hybrid media may have MBR AND an independent whole-device RDB.
  * Discover both views, warn on overlapping data extents, NEVER infer a
  * safe write target from this inspection. Invalid disk-wide RDB must not
  * hide an otherwise valid primary MBR. */
 rs=dolly_rdb_scan(source,0,block_bytes,16,&out->whole_disk_rdb);
 if(rs==DOLLY_RDB_OK){
  out->has_whole_disk_rdb=1;
  out->hybrid_mbr_rdb=1;
  for(i=0;i<out->whole_disk_rdb.partition_count;i++){
   const DollyPartition *r=&out->whole_disk_rdb.partitions[i];
   for(j=0;j<out->partition_count;j++){
    const DollyMbrPartition *m=&out->primary[j];
    if(overlaps(r->start_bytes,r->length_bytes,m->start_bytes,m->length_bytes))
     out->hybrid_mbr_overlap=1;
   }
  }
 }else if(rs!=DOLLY_RDB_NOT_FOUND){
  /* Do not silently hide a corrupt disk-wide RDB behind a valid MBR. */
  out->suspect_whole_disk_rdb=1;
 }
 return DOLLY_MAP_OK;
}
const char *dolly_map_status_name(DollyMapStatus s) {
 switch(s){
 case DOLLY_MAP_OK:return "OK";
 case DOLLY_MAP_NO_TABLE:return "No recognized partition map";
 case DOLLY_MAP_BAD_ARGUMENT:return "Bad arguments";
 case DOLLY_MAP_READ_ERROR:return "Read error";
 case DOLLY_MAP_OUT_OF_RANGE:return "Out-of-range partition";
 case DOLLY_MAP_OVERLAP:return "Overlapping partitions or metadata";
 case DOLLY_MAP_UNSUPPORTED:return "Unsupported MBR/GPT/EBR layout";
 case DOLLY_MAP_NESTED_RDB_ERROR:return "Invalid nested RDB";
 default:return "Unknown map error";
 }
}
