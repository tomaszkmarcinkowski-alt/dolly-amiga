#include "dolly_selfcheck.h"
#include "dolly_checksum.h"
#include "dolly_version.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct MemoryReader {
    const unsigned char *bytes;
    size_t size,position;
    int fail;
} MemoryReader;
static long memory_read(void *opaque,void *dst,size_t cap){
    MemoryReader *r=(MemoryReader *)opaque;
    size_t n;
    if(r->fail)return -1;
    if(r->position==r->size)return 0;
    n=r->size-r->position;
    if(n>cap)n=cap;
    if(n>3)n=3; /* ensures magic/CRC are split across read boundaries */
    memcpy(dst,r->bytes+r->position,n);
    r->position+=n;
    return (long)n;
}
int main(void){
    unsigned char file[96]={0};
    size_t pos=11;
    DollyChecksum crc;
    DollySelfCheckReport got;
    MemoryReader r;
    char checksum_hex[9];
    assert(strstr(dolly_version_text(),"$VER: Dolly ")!=0);
    memset(file,'x',sizeof(file));
    memcpy(file+pos,dolly_selfcheck_record,DOLLY_SELF_RECORD_SIZE);
    memcpy(file+pos+DOLLY_SELF_RELEASE_OFFSET,"Dolly-M0-Test",13);
    memset(file+pos+DOLLY_SELF_RELEASE_OFFSET+13,' ',3);
    dolly_checksum_init(&crc);
    assert(dolly_checksum_update(&crc,file,sizeof(file))==0);
    snprintf(checksum_hex,sizeof(checksum_hex),"%08X",
             (unsigned)dolly_checksum_final(&crc));
    memcpy(file+pos+DOLLY_SELF_CRC_OFFSET,checksum_hex,8);
    r.bytes=file;r.size=sizeof(file);r.position=0;r.fail=0;
    got=dolly_selfcheck_embedded_stream(memory_read,&r,file+pos);
    assert(got.result==DOLLY_SELF_OK);
    assert(got.actual_crc32==dolly_checksum_final(&crc));
    /* All other bytes, including release label, are part of the checksum. */
    file[6]^=1;r.position=0;
    got=dolly_selfcheck_embedded_stream(memory_read,&r,file+pos);
    assert(got.result==DOLLY_SELF_CRC_MISMATCH);
    file[6]^=1;
    /* CRC bytes themselves are ignored ONLY for checksum calculation. */
    file[pos+DOLLY_SELF_CRC_OFFSET]='F';r.position=0;
    got=dolly_selfcheck_embedded_stream(memory_read,&r,file+pos);
    assert(got.result==DOLLY_SELF_CRC_MISMATCH);
    memcpy(file+pos+DOLLY_SELF_CRC_OFFSET,checksum_hex,8);
    /* Disk-side metadata different from loaded metadata is rejected. */
    file[pos+DOLLY_SELF_RELEASE_OFFSET]^=1;r.position=0;
    got=dolly_selfcheck_embedded_stream(memory_read,&r,dolly_selfcheck_record);
    assert(got.result==DOLLY_SELF_RECORD_UNSTAMPED);
    file[pos+DOLLY_SELF_RELEASE_OFFSET]^=1;
    r.fail=1;
    got=dolly_selfcheck_embedded_stream(memory_read,&r,file+pos);
    assert(got.result==DOLLY_SELF_READ_FAILED);
    r.fail=0;
    file[pos]^=1;r.position=0;
    got=dolly_selfcheck_embedded_stream(memory_read,&r,file+pos);
    assert(got.result==DOLLY_SELF_RECORD_INVALID);
    puts("Embedded in-executable CRC32 verification tests passed");
    return 0;
}
