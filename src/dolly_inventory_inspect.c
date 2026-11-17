/* Sheep Inventory: host read-only prototype for RDB PART objects. */
#include "dolly_partition_map.h"
#include "dolly_inventory.h"
#include "dolly_maxtransfer.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int emit_field(const DollyInventoryField *f,void *ctx){
    FILE *out=(FILE *)ctx;
    const char *source=f->origin==DOLLY_INV_DERIVED?"computed":
        f->origin==DOLLY_INV_MBR?"MBR":
        f->origin==DOLLY_INV_RDB_HEADER?"RDSK":"RDB PART";
    if(f->availability!=DOLLY_INV_PRESENT){
        fprintf(out,"  %-23s %s (source: %s)\n",f->label,
                dolly_inventory_availability_name(f->availability),source);
        return 0;
    }
    if(f->kind==DOLLY_INV_TEXT){
        fprintf(out,"  %-23s %s (source: %s)\n",f->label,f->text,source);
    }else if(!strcmp(f->unit,"hex32")){
        fprintf(out,"  %-23s 0x%08" PRIX32 " (source: %s)\n",f->label,(uint32_t)f->number,source);
    }else{
        fprintf(out,"  %-23s %" PRIu64 " %s (source: %s)\n",f->label,f->number,f->unit,source);
    }
    if(!strcmp(f->key,"max_transfer")){
        DollyMaxTransferClass category=dolly_maxtransfer_classify(
            (uint32_t)f->number,f->availability==DOLLY_INV_PRESENT);
        if(category==DOLLY_MT_ABOVE_REFERENCE ||
           category==DOLLY_MT_INVALID_ZERO ||
           category==DOLLY_MT_BELOW_REFERENCE)
            fprintf(out,"    WARNING/NOTE: %s\n",dolly_maxtransfer_guidance(category));
    }
    return ferror(out)?1:0;
}
static void show_rdb(const DollyRdb *r,const char *type){
    unsigned i;
    printf("%s: RDSK block %u, partitions %u\n",type,r->header_block,r->partition_count);
    if(dolly_inventory_rdb_header(r,emit_field,stdout)!=0)
        fprintf(stderr,"RDSK inventory rendering stopped\n");
    for(i=0;i<r->partition_count;i++){
        printf("PART #%u\n",i+1);
        if(dolly_inventory_partition(&r->partitions[i],emit_field,stdout)!=0)
            fprintf(stderr,"Inventory rendering stopped\n");
    }
}
static int parse_block(const char *s,uint32_t *n){
    char *end;unsigned long x;
    if(!s||!*s||*s=='-')return 0;
    errno=0;x=strtoul(s,&end,0);
    if(errno||*end||x>UINT32_MAX)return 0;
    *n=(uint32_t)x;
    return 1;
}
int main(int argc,char **argv){
    DollyBlockSource *source=0;
    DollyPartitionMap *map=0;
    DollyMapStatus status;
    uint32_t sector=512;unsigned i;
    if(argc<2||argc>3||(argc==3&&!parse_block(argv[2],&sector))){
        fprintf(stderr,"Usage: dolly-inventory IMAGE [BLOCK_BYTES]\n");return 20;
    }
    if(dolly_block_open_file(argv[1],&source)!=DOLLY_BLOCK_OK){
        fprintf(stderr,"Cannot open image read-only\n");return 20;
    }
    map=(DollyPartitionMap*)calloc(1,sizeof(*map));
    if(!map){dolly_block_close(source);return 20;}
    status=dolly_partition_map_scan(source,sector,map);
    if(status!=DOLLY_MAP_OK){
        fprintf(stderr,"Inventory: %s\n",dolly_map_status_name(status));
        free(map);dolly_block_close(source);return 10;
    }
    puts("Dolly Sheep Inventory — experimental, read-only");
    printf("Image size: %" PRIu64 " bytes\n",dolly_block_size(source));
    for(i=0;i<map->partition_count;i++){
        const DollyMbrPartition *p=&map->primary[i];
        printf("MBR primary #%u: type=0x%02X start=%" PRIu64 " length=%" PRIu64 "\n",
               i+1,p->type,p->start_bytes,p->length_bytes);
        if(dolly_inventory_mbr_partition(p,emit_field,stdout)!=0)
            fprintf(stderr,"MBR inventory rendering stopped\n");
        if(p->contains_rdb)show_rdb(&p->nested_rdb,"Nested RDB");
    }
    if(map->has_whole_disk_rdb)show_rdb(&map->whole_disk_rdb,"Whole-disk RDB");
    if(map->hybrid_mbr_rdb || map->suspect_whole_disk_rdb)
        puts("WARNING: hybrid/suspect whole-device RDB; do not modify based on this report.");
    free(map);dolly_block_close(source);
    return 0;
}
