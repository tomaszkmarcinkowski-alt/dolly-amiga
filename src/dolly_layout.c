/* Read-only host CLI for primary MBR and nested Amiga RDB inspection. */
#include "dolly_partition_map.h"
#include "dolly_rdb_audit.h"
#include "dolly_capacity_audit.h"
#include "dolly_maxtransfer.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
static int parse_sector(const char *s,uint32_t *result){
 char *end;unsigned long n;
 if(!s||!*s||*s=='-')return 0;
 errno=0;n=strtoul(s,&end,0);
 if(errno||*end!='\0'||n>UINT32_MAX)return 0;
 *result=(uint32_t)n;return 1;
}
static void show_rdb(const DollyRdb *r,const char *indent,
                     uint64_t container_start,uint64_t container_length){
 unsigned i;
 printf("%sRDB block %u, %u B/block, %u partition(s)\n",
        indent,r->header_block,r->block_bytes,r->partition_count);
 {
  DollyCapacityAudit cap=dolly_capacity_audit(r,container_start,container_length);
  if(cap.flags&DOLLY_CAPACITY_GEOMETRY_SMALLER)
    printf("%sNOTICE: RDB geometry describes %" PRIu64
           " bytes fewer than the observed container; tail is NOT certified free.\n",
           indent,cap.extra_bytes);
  if(cap.flags&DOLLY_CAPACITY_GEOMETRY_LARGER)
    printf("%sWARNING: RDB geometry exceeds observed container by %" PRIu64
           " bytes; inspect before any write.\n",indent,cap.shortfall_bytes);
  if(cap.flags&DOLLY_CAPACITY_PART_OUTSIDE)
    printf("%sWARNING: PART #%u extends beyond observed container; disk writes unsafe.\n",
           indent,cap.first_outside_partition+1u);
  if(cap.flags&DOLLY_CAPACITY_META_OUTSIDE)
    printf("%sWARNING: RDB header or PART descriptor lies beyond container.\n",indent);
  if(cap.flags&DOLLY_CAPACITY_PART_PAST_GEOM)
    printf("%sWARNING: partition extends beyond RDB-declared geometry.\n",indent);
  if(cap.flags&(DOLLY_CAPACITY_GEOMETRY_UNKNOWN|DOLLY_CAPACITY_OVERFLOW|
                DOLLY_CAPACITY_INVALID))
    printf("%sWARNING: cannot determine trustworthy RDB geometry/capacity comparison.\n",indent);
 }
 {
  DollyRdbAudit audit=dolly_rdb_audit(r,container_start,container_length);
  if(audit.flags)
   printf("%sWARNING: RDB overlaps/invalid bounds (flags 0x%08" PRIX32
          "; partition indices %u/%u); do not modify\n",
          indent,audit.flags,audit.first_partition,audit.second_partition);
 }

 for(i=0;i<r->partition_count;i++){
  const DollyPartition *p=&r->partitions[i];
  printf("%s  %-12s start=%" PRIu64 " bytes, length=%" PRIu64
         " bytes, DOS type=%08" PRIX32 "\n",indent,p->name,p->start_bytes,
         p->length_bytes,p->dos_type);
  if(p->sectors_per_block!=1)
   printf("%s    WARNING: unexpected de_SectorPerBlock=%" PRIu32
          " (expected 1); review geometry\n",indent,p->sectors_per_block);
  {
   DollyMaxTransferClass level=
       dolly_maxtransfer_classify(p->max_transfer_bytes,p->max_transfer_present);
   if(p->max_transfer_present)
    printf("%s    MaxTransfer 0x%08" PRIX32 " (%" PRIu32 " B): %s\n",
           indent,p->max_transfer_bytes,p->max_transfer_bytes,
           dolly_maxtransfer_class_name(level));
   else printf("%s    MaxTransfer: unavailable\n",indent);
   if(level==DOLLY_MT_ABOVE_REFERENCE ||
      level==DOLLY_MT_INVALID_ZERO || level==DOLLY_MT_BELOW_REFERENCE)
    printf("%s    %s\n",indent,dolly_maxtransfer_guidance(level));
  }
 }
}
int main(int argc,char **argv){
 DollyBlockSource *src=NULL;DollyPartitionMap *map;DollyMapStatus status;
 uint32_t sector=512;unsigned i;
 if(argc<2||argc>3||(argc==3&&!parse_sector(argv[2],&sector))){
  fprintf(stderr,"Usage: dolly-layout disk.img [DEVICE_SECTOR_BYTES]\n");
  return 20;
 }
 if(dolly_block_open_file(argv[1],&src)!=DOLLY_BLOCK_OK){
  fprintf(stderr,"Cannot open %s read-only\n",argv[1]);return 20;
 }
 /* Do not place large RDB result arrays on small Amiga-like stacks. */
 map=(DollyPartitionMap *)calloc(1,sizeof(*map));
 if(!map){fprintf(stderr,"Out of memory\n");dolly_block_close(src);return 20;}
 status=dolly_partition_map_scan(src,sector,map);
 if(status!=DOLLY_MAP_OK){
  fprintf(stderr,"Map: %s\n",dolly_map_status_name(status));
  free(map);dolly_block_close(src);return 10;
 }
 printf("Dolly layout inspector (read-only; metadata validation is incomplete)\n");
 printf("Image bytes=%" PRIu64 ", logical sector=%" PRIu32 "\n",dolly_block_size(src),sector);
 if(map->has_mbr){
  printf("MBR: %u primary partition(s)\n",map->partition_count);
  for(i=0;i<map->partition_count;i++){
   const DollyMbrPartition *p=&map->primary[i];
   printf("  type=%02" PRIX8 " LBA=%" PRIu32 " count=%" PRIu32 " start=%" PRIu64 " length=%" PRIu64 "\n",
          p->type,p->first_lba,p->sector_count,p->start_bytes,p->length_bytes);
   if(p->contains_rdb)show_rdb(&p->nested_rdb,"    ",p->start_bytes,p->length_bytes);
  }
 }
 if(map->suspect_whole_disk_rdb)
  puts("WARNING: invalid disk-wide RDB candidate detected alongside MBR; inspect metadata before changing disk.");
 if(map->hybrid_mbr_rdb){
  puts("WARNING: both MBR and disk-wide RDB are present; this is an ambiguous hybrid layout.");
  if(map->hybrid_mbr_overlap)
   puts("WARNING: disk-wide RDB data overlaps MBR partitions. Do not write or clone automatically.");
 }
 if(map->has_whole_disk_rdb)show_rdb(&map->whole_disk_rdb,"  ",0,dolly_block_size(src));
 free(map);dolly_block_close(src);
 puts("NOTE: not a complete disk-integrity or bootability check.");return 0;
}
