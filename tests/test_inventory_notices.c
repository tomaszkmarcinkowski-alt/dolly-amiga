#include "dolly_inventory_notices.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct State {
    unsigned count,warnings,infos,maxwarnings,sectorwarnings,maskwarnings,collisions;
} State;
static int collect(const DollyInventoryNotice *n,void *ctx){
    State *s=(State *)ctx;s->count++;
    assert(n->code&&n->message&&*n->code&&*n->message);
    assert(n->suggested_action && strlen(n->suggested_action)>20);
    assert(strstr(n->suggested_action,"automatically") == NULL ||
           !strcmp(n->code,"MAXTRANSFER_BELOW_REFERENCE"));

    if(n->severity==DOLLY_NOTICE_WARNING)s->warnings++;
    if(n->severity==DOLLY_NOTICE_INFO)s->infos++;
    if(!strcmp(n->code,"MAXTRANSFER_ABOVE_REFERENCE"))s->maxwarnings++;
    if(!strcmp(n->code,"SECTORS_PER_BLOCK_NONSTANDARD"))s->sectorwarnings++;
    if(!strcmp(n->code,"MEMORY_MASK_ZERO"))s->maskwarnings++;
    if(!strcmp(n->code,"RDB_PARTITIONS_OVERLAP"))s->collisions++;
    return 0;
}
static int stop(const DollyInventoryNotice *n,void *ctx){
    unsigned *x=(unsigned *)ctx;(void)n;(*x)++;return 17;
}
int main(void){
    DollyPartition p;State s;unsigned early=0;
    memset(&p,0,sizeof(p));memset(&s,0,sizeof(s));
    p.sectors_per_block=1;p.max_transfer_present=1;
    p.max_transfer_bytes=0x1fe00u;p.dos_type_present=1;
    assert(dolly_inventory_partition_notices(&p,collect,&s)==0);
    assert(s.count==0);
    p.max_transfer_bytes=0x7fffffffu;
    p.sectors_per_block=0;p.dma_mask_present=1;p.dma_mask=0;
    assert(dolly_inventory_partition_notices(&p,collect,&s)==0);
    assert(s.warnings==3&&s.maxwarnings==1&&s.sectorwarnings==1&&s.maskwarnings==1);
    assert(dolly_inventory_partition_notices(&p,stop,&early)==17&&early==1);
    memset(&s,0,sizeof(s));
    p.max_transfer_present=0;p.sectors_per_block=1;p.dma_mask=0xffffffffu;p.dos_type_present=0;
    assert(dolly_inventory_partition_notices(&p,collect,&s)==0);
    assert(s.count==2&&s.infos==2);
    {
        DollyRdb rdb;
        memset(&rdb,0,sizeof(rdb));
        memset(&s,0,sizeof(s));
        rdb.block_bytes=512;rdb.header_block=1;rdb.partition_count=2;
        rdb.partitions[0].descriptor_block=2;
        rdb.partitions[1].descriptor_block=3;
        rdb.partitions[0].start_bytes=100u*512u;
        rdb.partitions[0].length_bytes=10u*512u;
        rdb.partitions[1].start_bytes=105u*512u;
        rdb.partitions[1].length_bytes=10u*512u;
        assert(dolly_inventory_rdb_notices(&rdb,0,200u*512u,collect,&s)==0);
        assert(s.collisions==1&&s.count==1&&s.warnings==1);
        assert(dolly_inventory_rdb_notices(&rdb,0,200u*512u,stop,&early)==17);
        rdb.partitions[1].start_bytes=150u*512u;
        memset(&s,0,sizeof(s));
        assert(dolly_inventory_rdb_notices(&rdb,0,200u*512u,collect,&s)==0);
        assert(s.count==0);
    }
    assert(dolly_inventory_partition_notices(NULL,collect,&s)==-1);
    assert(!strcmp(dolly_notice_severity_name(DOLLY_NOTICE_WARNING),"WARNING"));
    puts("Inventory advisory tests passed");return 0;
}
