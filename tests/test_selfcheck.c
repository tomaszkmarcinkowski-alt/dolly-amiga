#include "dolly_selfcheck.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct Reader {
    const unsigned char *bytes;
    size_t size,position;
    int fail_after_first;
} Reader;
static long read_chunk(void *p,void *dest,size_t cap){
    Reader *r=(Reader *)p;
    size_t available,amount;
    if(r->fail_after_first && r->position>0)return -1;
    if(r->position>=r->size)return 0;
    available=r->size-r->position;
    amount=available<cap?available:cap;
    if(amount>3)amount=3;
    memcpy(dest,r->bytes+r->position,amount);
    r->position+=amount;
    return (long)amount;
}
int main(void){
    const unsigned char sample[]="123456789";
    const unsigned char modified[]="12345678x";
    Reader r={sample,9,0,0};
    DollySelfCheckExpected expected={"Dolly-build-test","Dolly-build-test",
                                      9,0xcbf43926u};
    DollySelfCheckReport got=dolly_selfcheck_stream(read_chunk,&r,expected);
    assert(got.result==DOLLY_SELF_OK);
    assert(got.actual_size==9 && got.actual_crc32==0xcbf43926u);
    r.bytes=modified;r.position=0;
    got=dolly_selfcheck_stream(read_chunk,&r,expected);
    assert(got.result==DOLLY_SELF_CRC_MISMATCH);
    expected.size=8;r.position=0;
    got=dolly_selfcheck_stream(read_chunk,&r,expected);
    assert(got.result==DOLLY_SELF_SIZE_MISMATCH);
    expected.size=9;
    expected.manifest_release="other-build";r.position=0;
    got=dolly_selfcheck_stream(read_chunk,&r,expected);
    assert(got.result==DOLLY_SELF_VERSION_MISMATCH && r.position==0);
    expected.manifest_release="Dolly-build-test";
    r.position=0;r.fail_after_first=1;
    got=dolly_selfcheck_stream(read_chunk,&r,expected);
    assert(got.result==DOLLY_SELF_READ_FAILED);
    got=dolly_selfcheck_stream(NULL,NULL,expected);
    assert(got.result==DOLLY_SELF_INVALID);
    assert(strstr(dolly_selfcheck_message(DOLLY_SELF_CRC_MISMATCH),"Reinstall")!=NULL);
    puts("In-process executable CRC32 self-check decision tests passed");
    return 0;
}
