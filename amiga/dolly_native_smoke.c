/* Dolly native 68000 smoke test. No device commands or disk writes.
 * Build with an AmigaOS C99-capable cross compiler and SDK.
 * The same program can be run on a POSIX host to test its logic.
 */
#include "dolly_block_source.h"
#include "dolly_rdb.h"
#include "dolly_rdb_audit.h"
#include "dolly_inventory.h"
#include "dolly_io_plan.h"
#include "dolly_maxtransfer.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SMOKE_BS 512u
#define SMOKE_CAPACITY 4096u
static unsigned char disk_bytes[SMOKE_CAPACITY];

static void put_be32(unsigned char *b, uint32_t value) {
    b[0]=(unsigned char)(value>>24);
    b[1]=(unsigned char)(value>>16);
    b[2]=(unsigned char)(value>>8);
    b[3]=(unsigned char)value;
}
static uint32_t read_be32(const unsigned char *b) {
    return ((uint32_t)b[0]<<24)|((uint32_t)b[1]<<16)|
           ((uint32_t)b[2]<<8)|(uint32_t)b[3];
}
static void seal_checksum(unsigned char *block, unsigned words) {
    uint32_t total=0;
    unsigned i;
    put_be32(block+4,words);
    put_be32(block+8,0);
    for(i=0;i<words;i++) total+=read_be32(block+4*i);
    put_be32(block+8,UINT32_C(0)-total);
}
static void build_fixture(void) {
    unsigned char *rdb=disk_bytes;
    unsigned char *part=disk_bytes+SMOKE_BS;
    memset(disk_bytes,0,sizeof(disk_bytes));
    memcpy(rdb,"RDSK",4);
    put_be32(rdb+16,SMOKE_BS);
    put_be32(rdb+28,1); /* PART linked-list head */
    put_be32(rdb+64,8); put_be32(rdb+68,1); put_be32(rdb+72,1);
    seal_checksum(rdb,64);
    memcpy(part,"PART",4);
    put_be32(part+16,UINT32_MAX);
    part[36]=3;
    memcpy(part+37,"DH0",3);
    put_be32(part+128,17);
    put_be32(part+132,128); /* longwords per filesystem block */
    put_be32(part+140,1);   /* heads */
    put_be32(part+144,1);   /* sectors per block */
    put_be32(part+148,1);   /* blocks per track */
    put_be32(part+164,4);   /* first cylinder => byte offset 2048 */
    put_be32(part+168,4);
    put_be32(part+128+11*4,30);  /* de_NumBuffers */
    put_be32(part+128+12*4,0);   /* de_BufMemType */
    put_be32(part+128+13*4,0x1fe00); /* de_MaxTransfer */
    put_be32(part+128+14*4,0x00fffffe); /* de_Mask */
    put_be32(part+192,0x444f5301);  /* de_DosType at 128+16*4 */
    seal_checksum(part,64);
}
static DollyBlockReadResult mem_read(void *context,uint64_t offset,
                                    void *destination,size_t length){
    DollyBlockReadResult result;
    const unsigned char *memory=(const unsigned char *)context;
    result.status=DOLLY_BLOCK_EIO;
    result.bytes_read=0;
    if(offset>SMOKE_CAPACITY || length>SMOKE_CAPACITY-(size_t)offset)
        return result;
    memcpy(destination,memory+(size_t)offset,length);
    result.status=DOLLY_BLOCK_OK;
    result.bytes_read=length;
    return result;
}
static void mem_close(void *unused){(void)unused;}
static int field_count(const DollyInventoryField *field,void *context){
    unsigned *count=(unsigned *)context;
    if(!field->key || !field->label)return 1;
    (*count)++;
    return 0;
}
static int run_selftest(void){
    DollyBlockSource *source=0;
    DollyRdb *rdb=0;
    DollyRdbStatus parsed;
    DollyRdbAudit audit;
    DollyIoReadPlan plan;
    DollyBlockStatus status;
    unsigned fields=0;
    int success=0;
    build_fixture();
    status=dolly_block_open_backend(sizeof(disk_bytes),mem_read,mem_close,
                                    disk_bytes,&source);
    if(status!=DOLLY_BLOCK_OK)goto cleanup;
    /* Allocate RDB result on the heap, never on the Amiga task stack. */
    rdb=(DollyRdb *)malloc(sizeof(*rdb));
    if(!rdb)goto cleanup;
    parsed=dolly_rdb_scan(source,0,SMOKE_BS,4,rdb);
    if(parsed!=DOLLY_RDB_OK || rdb->partition_count!=1)goto cleanup;
    if(strcmp(rdb->partitions[0].name,"DH0")!=0 ||
       rdb->partitions[0].start_bytes!=2048 ||
       rdb->partitions[0].length_bytes!=512)goto cleanup;
    if(!rdb->partitions[0].max_transfer_present ||
       rdb->partitions[0].max_transfer_bytes!=0x1fe00)goto cleanup;
    audit=dolly_rdb_audit(rdb,0,SMOKE_CAPACITY);
    if(audit.flags)goto cleanup;
    if(dolly_maxtransfer_classify(rdb->partitions[0].max_transfer_bytes,1)
       !=DOLLY_MT_AT_REFERENCE)goto cleanup;
    if(dolly_inventory_partition(&rdb->partitions[0],field_count,&fields)!=0
       || fields<10)goto cleanup;
    if(dolly_io_plan_read(UINT64_C(0x100000000),512,512,4096,0,&plan)
       !=DOLLY_BLOCK_EUNSUPPORTED)goto cleanup;
    if(dolly_io_plan_read(UINT64_C(0x100000000),512,512,4096,1,&plan)
       !=DOLLY_BLOCK_OK || plan.offset_high!=1 ||
       plan.command!=DOLLY_NSCMD_TD_READ64)goto cleanup;
    success=1;
cleanup:
    if(rdb)free(rdb);
    if(source)dolly_block_close(source);
    return success;
}
int main(int argc,char **argv){
    if(argc==2 && strcmp(argv[1],"--version")==0){
        puts("Dolly M0 native smoke (read-only)");return 0;
    }
    if(argc==2 && strcmp(argv[1],"--selftest")==0){
        int result=run_selftest();
        puts(result?"Dolly native smoke: PASS":"Dolly native smoke: FAIL");
        return result?0:10;
    }
    puts("Usage: dolly-native-smoke --version | --selftest | --help");
    puts("No physical device is accessed; no disk writes are possible.");
    return argc==1 || (argc==2 && strcmp(argv[1],"--help")==0)?0:20;
}
