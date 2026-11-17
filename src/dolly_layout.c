/* Read-only host CLI for primary MBR and nested Amiga RDB inspection. */
#include "dolly_partition_map.h"
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
static void show_rdb(const DollyRdb *r,const char *indent){
 unsigned i;
 printf("%sRDB block %u, %u B/block, %u partition(s)\n",
        indent,r->header_block,r->block_bytes,r->partition_count);
 for(i=0;i<r->partition_count;i++){
  const DollyPartition *p=&r->partitions[i];
  printf("%s  %-12s start=%" PRIu64 " bytes, length=%" PRIu64
         " bytes, DOS type=%08" PRIX32 "\n",indent,p->name,p->start_bytes,
         p->length_bytes,p->dos_type);
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
   if(p->contains_rdb)show_rdb(&p->nested_rdb,"    ");
  }
 }
 if(map->has_whole_disk_rdb)show_rdb(&map->whole_disk_rdb,"  ");
 free(map);dolly_block_close(src);
 puts("NOTE: not a complete disk-integrity or bootability check.");return 0;
}
