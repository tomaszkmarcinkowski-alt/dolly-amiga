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
} InventoryCheck;
static int collect(const DollyInventoryField *f,void *user){
    InventoryCheck *c=(InventoryCheck *)user;
    c->received++;
    if(f->origin==DOLLY_INV_DERIVED)c->derived_count++;
    if(!strcmp(f->key,"name")){
        assert(f->kind==DOLLY_INV_TEXT);
        assert(!strcmp(f->text,"DH0"));c->name_seen++;
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
    assert(dolly_inventory_partition(&p,collect,&c)==0);
    assert(c.received==9&&c.max_seen==1&&c.name_seen==1&&c.derived_count==3);
    assert(c.max_unavailable==0&&c.overflow_seen==0);
    memset(&c,0,sizeof(c));p.max_transfer_present=0;
    p.start_bytes=UINT64_MAX;p.length_bytes=512;
    assert(dolly_inventory_partition(&p,collect,&c)==0);
    assert(c.max_unavailable==1&&c.overflow_seen==1);
    assert(dolly_inventory_partition(&p,stop_early,&n)==7&&n==1);
    assert(dolly_inventory_partition(0,collect,&c)==-1);
    assert(dolly_inventory_partition(&p,0,&c)==-1);
    puts("Sheep Inventory streaming field tests passed");return 0;
}
