#include "dolly_rdb.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#define MIN_RDB_BYTES 512u
#define MAX_RDB_BYTES 32768u
static uint32_t be32(const unsigned char *p) {
 return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static int mul_u64(uint64_t a,uint64_t b,uint64_t *out) {
 if(b && a>UINT64_MAX/b)return 0;
 *out=a*b;return 1;
}
static int add_u64(uint64_t a,uint64_t b,uint64_t *out) {
 if(a>UINT64_MAX-b)return 0;
 *out=a+b;return 1;
}
static int valid_block_size(uint32_t v) {
 return v>=MIN_RDB_BYTES && v<=MAX_RDB_BYTES && (v&(v-1))==0;
}
static int checksum(const unsigned char *data,uint32_t block_bytes,uint32_t min_words) {
 uint32_t n=be32(data+4),sum=0,i;
 /* All parser-consumed fields must be covered by the checksum. */
 if(n<min_words || n>block_bytes/4)return 0;
 for(i=0;i<n;i++)sum+=be32(data+i*4);
 return sum==0;
}
static DollyRdbStatus load_block(DollyBlockSource *src,uint64_t base,
 uint32_t bs,uint32_t block,unsigned char *data) {
 uint64_t offset;
 DollyBlockReadResult r;
 if(!mul_u64((uint64_t)block,bs,&offset)||!add_u64(base,offset,&offset))return DOLLY_RDB_OUT_OF_RANGE;
 if(offset>dolly_block_size(src) || (uint64_t)bs>dolly_block_size(src)-offset)return DOLLY_RDB_OUT_OF_RANGE;
 r=dolly_block_read(src,offset,data,bs);
 return (r.status==DOLLY_BLOCK_OK && r.bytes_read==bs)?DOLLY_RDB_OK:DOLLY_RDB_READ_ERROR;
}
static DollyRdbStatus parse_partitions(DollyBlockSource *src,DollyRdb *out,unsigned char *buf){
 uint32_t current=out->partition_head,visited[DOLLY_RDB_MAX_PARTITIONS];
 unsigned seen=0;
 while(current!=DOLLY_RDB_END){
  DollyRdbStatus s; uint32_t next,low,high,surfaces,bpt,dos,size_block,env_table_size;
  uint64_t cyl_bytes,start,length,end,relative,fs_block_bytes;
  unsigned i;unsigned char name_len;
  if(seen>=DOLLY_RDB_MAX_PARTITIONS)return DOLLY_RDB_TOO_MANY;
  for(i=0;i<seen;i++)if(visited[i]==current)return DOLLY_RDB_CYCLE;
  visited[seen++]=current;
  s=load_block(src,out->base_offset,out->block_bytes,current,buf);if(s!=DOLLY_RDB_OK)return s;
  if(memcmp(buf,"PART",4)!=0)return DOLLY_RDB_BAD_BLOCK;
  /* We cannot trust de_TableSize before the checksum, but we can reject
   * out-of-buffer values and demand enough checksummed words to cover
   * every declared field we will read. */
  env_table_size=be32(buf+128);
  if(env_table_size<11 || env_table_size>=(out->block_bytes-128)/4)
    return DOLLY_RDB_BAD_BLOCK;
  if(!checksum(buf,out->block_bytes,33u+env_table_size))
    return DOLLY_RDB_BAD_CHECKSUM;
  next=be32(buf+16);
  size_block=be32(buf+128+1*4);
  surfaces=be32(buf+128+3*4);
  bpt=be32(buf+128+5*4);
  low=be32(buf+128+9*4);
  high=be32(buf+128+10*4);
  dos=env_table_size>=16?be32(buf+128+16*4):0;
  if(!surfaces||!bpt||!size_block||high<low)return DOLLY_RDB_BAD_BLOCK;
  /* de_SizeBlock is in 32-bit longwords and may differ from the RDB
   * device block size. Account for it when calculating byte positions. */
  if(!mul_u64(size_block,4u,&fs_block_bytes) || fs_block_bytes < out->block_bytes ||
     fs_block_bytes % out->block_bytes != 0) return DOLLY_RDB_BAD_BLOCK;
  if(!mul_u64(surfaces,bpt,&cyl_bytes) || !mul_u64(cyl_bytes,fs_block_bytes,&cyl_bytes))return DOLLY_RDB_OUT_OF_RANGE;
  if(!mul_u64(low,cyl_bytes,&relative) || !add_u64(out->base_offset,relative,&start))return DOLLY_RDB_OUT_OF_RANGE;
  if(!mul_u64((uint64_t)high-low+1,cyl_bytes,&length) || !add_u64(start,length,&end))return DOLLY_RDB_OUT_OF_RANGE;
  if(end>dolly_block_size(src))return DOLLY_RDB_OUT_OF_RANGE;
  {
   DollyPartition *p=&out->partitions[out->partition_count++];
   memset(p,0,sizeof(*p));p->descriptor_block=current;
   p->low_cylinder=low;p->high_cylinder=high;p->dos_type=dos;
   /* de_TableSize describes the last valid DosEnvec index. */
   p->block_size_longwords=size_block;
   p->sectors_per_block=be32(buf+128+4*4);
   p->reserved_blocks=be32(buf+128+6*4);
   p->preallocated_blocks=be32(buf+128+7*4);
   p->buffer_count_present = env_table_size>=11;
   p->buffer_memory_type_present = env_table_size>=12;
   p->dma_mask_present = env_table_size>=14;
   p->boot_priority_present = env_table_size>=15;
   if(p->boot_priority_present) p->boot_priority=(int32_t)be32(buf+128+15*4);
   if(p->buffer_count_present) p->buffer_count=be32(buf+128+11*4);
   if(p->buffer_memory_type_present) p->buffer_memory_type=be32(buf+128+12*4);
   if(p->dma_mask_present) p->dma_mask=be32(buf+128+14*4);
   p->dos_type_present = env_table_size>=16;
   p->max_transfer_present = env_table_size>=13;
   if(p->max_transfer_present) p->max_transfer_bytes=be32(buf+128+13*4);
   p->start_bytes=start;p->length_bytes=length;
   name_len=buf[36];if(name_len>31)name_len=31;
   memcpy(p->name,buf+37,name_len);p->name[name_len]=0;
  }
  current=next;
 }
 return DOLLY_RDB_OK;
}
DollyRdbStatus dolly_rdb_scan(DollyBlockSource *src,uint64_t base,uint32_t bs,uint32_t scan_blocks,DollyRdb *out){
 unsigned char *buf;uint32_t n;DollyRdbStatus found=DOLLY_RDB_NOT_FOUND;
 int saw_corrupt_checksum=0, saw_invalid_size=0;
 if(!src||!out||!valid_block_size(bs)||scan_blocks==0||scan_blocks>4096)return DOLLY_RDB_BAD_ARGUMENT;
 memset(out,0,sizeof(*out));out->base_offset=base;out->block_bytes=bs;
 buf=(unsigned char*)malloc(bs);if(!buf)return DOLLY_RDB_READ_ERROR;
 for(n=0;n<scan_blocks;n++){
  DollyRdbStatus s=load_block(src,base,bs,n,buf);
  if(s==DOLLY_RDB_OUT_OF_RANGE)break;
  if(s!=DOLLY_RDB_OK){found=s;break;}
  if(memcmp(buf,"RDSK",4)!=0)continue;
  if(!checksum(buf,bs,19u)){saw_corrupt_checksum=1;continue;}
  if(be32(buf+16)!=bs){saw_invalid_size=1;continue;}
  out->header_block=n;out->partition_head=be32(buf+28);
  out->cylinders=be32(buf+64);out->sectors=be32(buf+68);out->heads=be32(buf+72);
  found=parse_partitions(src,out,buf);break;
 }
 free(buf);
 if(found==DOLLY_RDB_NOT_FOUND){
  if(saw_corrupt_checksum)return DOLLY_RDB_BAD_CHECKSUM;
  if(saw_invalid_size)return DOLLY_RDB_BAD_BLOCK;
 }
 return found;
}
const char *dolly_rdb_status_name(DollyRdbStatus s){
 switch(s){
 case DOLLY_RDB_OK:return "OK";
 case DOLLY_RDB_NOT_FOUND:return "RDSK not found";
 case DOLLY_RDB_BAD_ARGUMENT:return "Invalid scan parameters";
 case DOLLY_RDB_BAD_BLOCK:return "Invalid partition block";
 case DOLLY_RDB_BAD_CHECKSUM:return "Checksum mismatch";
 case DOLLY_RDB_OUT_OF_RANGE:return "Block or partition outside source";
 case DOLLY_RDB_CYCLE:return "Cycle in partition list";
 case DOLLY_RDB_TOO_MANY:return "Too many partitions";
 case DOLLY_RDB_READ_ERROR:return "Read failure";
 default:return "Unknown RDB error";
 }
}
