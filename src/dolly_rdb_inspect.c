/* Host-only read-only RDB inspector. */
#include "dolly_rdb.h"
#include "dolly_rdb_audit.h"
#include "dolly_capacity_audit.h"
#include "dolly_maxtransfer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
static int parse_u64(const char *s,uint64_t *v){
 char *end;unsigned long long n;
 if(!s||!*s||*s=='-')return 0;
 errno=0;n=strtoull(s,&end,0);
 if(errno||*end!='\0')return 0;
 *v=(uint64_t)n;return 1;
}
int main(int argc,char **argv){
 DollyBlockSource *source=NULL;DollyRdb r;DollyRdbStatus s;
 uint64_t base=0,bs=512;unsigned i;
 if(argc<2||argc>4|| (argc>=3&&!parse_u64(argv[2],&base)) ||
    (argc>=4&&!parse_u64(argv[3],&bs)) || bs>UINT32_MAX){
  fprintf(stderr,"Usage: dolly-rdb IMAGE [BASE_OFFSET_BYTES] [DEVICE_BLOCK_BYTES]\n");return 20;
 }
 if(dolly_block_open_file(argv[1],&source)!=DOLLY_BLOCK_OK){
  fprintf(stderr,"Cannot open image read-only: %s\n",argv[1]);return 20;
 }
 s=dolly_rdb_scan(source,base,(uint32_t)bs,16,&r);
 if(s!=DOLLY_RDB_OK){fprintf(stderr,"RDB: %s\n",dolly_rdb_status_name(s));dolly_block_close(source);return 10;}
 printf("Dolly RDB Inspector [read-only, experimental]\n");
 printf("RDSK base offset: %" PRIu64 " bytes; block: %u bytes; RDSK at block %u\n",
        base,r.block_bytes,r.header_block);
 printf("Geometry: %u cylinders, %u heads, %u sectors\n",r.cylinders,r.heads,r.sectors);
 printf("Partitions: %u\n",r.partition_count);
 {
  uint64_t source_bytes=dolly_block_size(source);
  if(base>=source_bytes){
   fprintf(stderr,"WARNING: RDB base outside the image.\n");
  }else{
   DollyCapacityAudit capacity=dolly_capacity_audit(&r,base,source_bytes-base);
   if(capacity.flags&DOLLY_CAPACITY_GEOMETRY_SMALLER)
    fprintf(stderr,"NOTICE: observed image is %" PRIu64
            " bytes larger than RDB geometry; tail is not certified free.\n",
            capacity.extra_bytes);
   if(capacity.flags&DOLLY_CAPACITY_GEOMETRY_LARGER)
    fprintf(stderr,"WARNING: RDB geometry exceeds observed image region by %" PRIu64
            " bytes.\n",capacity.shortfall_bytes);
   if(capacity.flags&DOLLY_CAPACITY_PART_OUTSIDE)
    fprintf(stderr,"WARNING: PART #%u exceeds observed image region.\n",
            capacity.first_outside_partition+1u);
   if(capacity.flags&(DOLLY_CAPACITY_META_OUTSIDE|
                       DOLLY_CAPACITY_PART_PAST_GEOM|
                       DOLLY_CAPACITY_GEOMETRY_UNKNOWN|
                       DOLLY_CAPACITY_OVERFLOW|DOLLY_CAPACITY_INVALID))
    fprintf(stderr,"WARNING: additional RDB geometry/capacity flags 0x%08" PRIX32
            "; see full layout inspector.\n",capacity.flags);
  }
 }
 {
  DollyRdbAudit audit=dolly_rdb_audit(&r,base,dolly_block_size(source)-base);
  if(audit.flags)
   fprintf(stderr,"WARNING: RDB collision audit flags=0x%08" PRIX32
           " (partition indices %u / %u); not safe to modify\n",
           audit.flags,audit.first_partition,audit.second_partition);
 }

 for(i=0;i<r.partition_count;i++){
  DollyPartition *p=&r.partitions[i];
  printf("  %s: PART=%u cylinders=%u..%u start=%" PRIu64 " length=%" PRIu64 " DosType=%08" PRIX32 "\n",
         p->name,p->descriptor_block,p->low_cylinder,p->high_cylinder,p->start_bytes,p->length_bytes,p->dos_type);
  if(p->sectors_per_block!=1)
   fprintf(stderr,"WARNING: %s de_SectorPerBlock=%" PRIu32
           " (expected 1); verify geometry\n",p->name,p->sectors_per_block);
  {
   DollyMaxTransferClass c=dolly_maxtransfer_classify(p->max_transfer_bytes,p->max_transfer_present);
   if(p->max_transfer_present){
    printf("    MaxTransfer: 0x%08" PRIX32 " (%" PRIu32 " bytes) [%s]\n",
           p->max_transfer_bytes,p->max_transfer_bytes,dolly_maxtransfer_class_name(c));
   }else printf("    MaxTransfer: not present [%s]\n",dolly_maxtransfer_class_name(c));
   printf("    Advice: %s\n",dolly_maxtransfer_guidance(c));
  }
 }
 dolly_block_close(source);return 0;
}
