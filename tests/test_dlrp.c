#include "dolly_dlrp.h"
#include <assert.h>
#include <string.h>

typedef struct Buffer {
    unsigned char bytes[32];
    size_t length;
} Buffer;

static int write_bytes(void *ctx,const void *data,size_t size) {
    Buffer *out=ctx;
    if(size>sizeof(out->bytes)-out->length)return 0;
    memcpy(out->bytes+out->length,data,size);
    out->length+=size;
    return 1;
}

int main(void) {
    Buffer out={{0},0};
    unsigned char integer[8];
    static const unsigned char expected[]={
        'F','O','R','M',0,0,0,16,'D','L','R','P',
        'S','R','C','E',0,0,0,3,'D','H','0',0
    };
    assert(!dolly_dlrp_form(write_bytes,&out,11));
    assert(dolly_dlrp_form(write_bytes,&out,12));
    assert(dolly_dlrp_chunk(write_bytes,&out,"SRCE","DH0",3));
    assert(out.length==sizeof(expected) && memcmp(out.bytes,expected,sizeof(expected))==0);
    assert(!dolly_dlrp_chunk(write_bytes,&out,"HEAD","x",1));
    dolly_dlrp_u64(integer,UINT64_C(0x0123456789abcdef));
    assert(memcmp(integer,"\x01\x23\x45\x67\x89\xab\xcd\xef",8)==0);
    return 0;
}
