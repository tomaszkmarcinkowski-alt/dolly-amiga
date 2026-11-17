#include "dolly_inventory.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct InventoryCheck {
    unsigned received;
    unsigned max_seen;
    unsigned max_unavailable;
    unsigned derived_count;
    unsigned name_seen;
    unsigned overflow_seen;
    unsigned dos_unavailable;
    unsigned mask_seen,mask_unavailable;
} InventoryCheck;
static int collect(const DollyInventoryField *f,void *user){
    InventoryCheck *c=(InventoryCheck *)user;
    c->received++;
    if(f->origin==DOLLY_INV_DERIVED)
        assert(f->evidence==DOLLY_EVIDENCE_DERIVED);
    else assert(f->evidence==DOLLY_EVIDENCE_DECLARED);
    if(f->origin==DOLLY_INV_DERIVED)c->derived_count++;
    if(!strcmp(f->key,"name")){
        assert(f->kind==DOLLY_INV_TEXT);
        assert(!strcmp(f->text,"DH0"));c->name_seen++;
    }
    if(!strcmp(f->key,"dos_type") &&
       f->availability==DOLLY_INV_UNAVAILABLE)c->dos_unavailable++;
    if(!strcmp(f->key,"dma_mask")){
        c->mask_seen++;
        if(f->availability==DOLLY_INV_UNAVAILABLE)c->mask_unavailable++;
        else assert(f->number==0x00fffffeu);
    }
    if(!strcmp(f->key,"max_transfer")){
        c->max_seen++;
        if(f->availability==DOLLY_INV_UNAVAILABLE)c->max_unavailable++;
        else assert(f->number==0x1fe00u);
    }
    if(!strcmp(f->key,"end_exclusive_bytes") &&
       f->availability==DOLLY_INV_UNAVAILABLE)c->overflow_seen++;
    return 0;
}
static int count_mbr(const DollyInventoryField *f,void *user){
    unsigned *count=(unsigned *)user;
    assert(f->origin==DOLLY_INV_MBR);
    assert(f->evidence==DOLLY_EVIDENCE_DECLARED);
    assert(f->kind==DOLLY_INV_U64);
    assert(f->availability==DOLLY_INV_PRESENT);
    if(!strcmp(f->key,"mbr_type"))assert(f->number==0x76u);
    if(!strcmp(f->key,"mbr_first_lba"))assert(f->number==4096u);
    if(!strcmp(f->key,"mbr_contains_rdb"))assert(f->number==1);
    ++*count;return 0;
}
static int count_rdb(const DollyInventoryField *f,void *user){
    unsigned *n=(unsigned *)user;
    assert(f->availability==DOLLY_INV_PRESENT);
    assert(f->evidence==(f->origin==DOLLY_INV_DERIVED?
                          DOLLY_EVIDENCE_DERIVED:DOLLY_EVIDENCE_DECLARED));
    if(!strcmp(f->key,"rdb_block_bytes"))assert(f->number==512);
    if(!strcmp(f->key,"rdb_partition_count"))assert(f->number==2);
    ++*n;return 0;
}
static int stop_early(const DollyInventoryField *f,void *user){
    unsigned *count=(unsigned *)user;(void)f;(*count)++;return 7;
}
int main(void){
    DollyPartition p;InventoryCheck c;unsigned n=0;
    memset(&p,0,sizeof(p));memset(&c,0,sizeof(c));
    strcpy(p.name,"DH0");p.descriptor_block=4;
    p.low_cylinder=10;p.high_cylinder=100;p.dos_type=0x444f5301;
    p.start_bytes=5120;p.length_bytes=1048576;
    p.max_transfer_present=1;p.max_transfer_bytes=0x1fe00;
    p.dos_type_present=1;
    p.buffer_count_present=1;p.buffer_count=30;
    p.buffer_memory_type_present=1;p.buffer_memory_type=0;
    p.dma_mask_present=1;p.dma_mask=0x00fffffeu;
    p.block_size_longwords=128;p.sectors_per_block=1;
    p.reserved_blocks=2;p.preallocated_blocks=0;
    assert(dolly_inventory_partition(&p,collect,&c)==0);
    assert(c.received==16&&c.mask_seen==1&&c.max_seen==1&&c.name_seen==1&&c.derived_count==3);
    assert(c.max_unavailable==0&&c.overflow_seen==0);
    memset(&c,0,sizeof(c));p.max_transfer_present=0;
    p.dos_type_present=0;
    p.dma_mask_present=0;
    p.start_bytes=UINT64_MAX;p.length_bytes=512;
    assert(dolly_inventory_partition(&p,collect,&c)==0);
    assert(c.max_unavailable==1&&c.overflow_seen==1&&c.dos_unavailable==1&&c.mask_unavailable==1);
    assert(dolly_inventory_partition(&p,stop_early,&n)==7&&n==1);
    {
        DollyMbrPartition m;
        unsigned fields=0;
        memset(&m,0,sizeof(m));
        m.type=0x76;m.first_lba=4096;m.sector_count=8192;
        m.start_bytes=4096u*512u;m.length_bytes=8192u*512u;
        m.contains_rdb=1;
        assert(dolly_inventory_mbr_partition(&m,stop_early,&n)==7);
        assert(n==2);
        /* Full MBR stream has seven fields and never allocates. */
        assert(dolly_inventory_mbr_partition(&m,count_mbr,&fields)==0);
        assert(fields==7);
        assert(dolly_inventory_mbr_partition(NULL,count_mbr,&fields)==-1);
    }
    {
        DollyRdb hdr;
        unsigned fields=0;
        memset(&hdr,0,sizeof(hdr));
        hdr.block_bytes=512;hdr.partition_count=2;
        hdr.cylinders=100;hdr.heads=4;hdr.sectors=16;
        assert(dolly_inventory_rdb_header(&hdr,count_rdb,&fields)==0);
        assert(fields==7);
        assert(dolly_inventory_rdb_header(&hdr,stop_early,&n)==7);
    }
    assert(!strcmp(dolly_inventory_evidence_name(DOLLY_EVIDENCE_DECLARED),"declared"));
    assert(!strcmp(dolly_inventory_evidence_name(DOLLY_EVIDENCE_OBSERVED),"observed"));
    assert(!strcmp(dolly_inventory_availability_name(DOLLY_INV_UNAVAILABLE),"not present"));
    assert(!strcmp(dolly_inventory_availability_name(DOLLY_INV_NOT_QUERIED),"not queried"));
    assert(!strcmp(dolly_inventory_availability_name(DOLLY_INV_UNSUPPORTED),"unsupported"));
    assert(!strcmp(dolly_inventory_availability_name(DOLLY_INV_INCONSISTENT),"inconsistent"));
    assert(dolly_inventory_partition(0,collect,&c)==-1);
    assert(dolly_inventory_partition(&p,0,&c)==-1);
    puts("Sheep Inventory streaming field tests passed");return 0;
}
