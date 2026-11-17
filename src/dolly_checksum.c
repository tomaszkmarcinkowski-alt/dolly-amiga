#include "dolly_checksum.h"
#include <stdint.h>
void dolly_checksum_init(DollyChecksum *s){
    if(s){s->crc=UINT32_C(0xffffffff);s->bytes=0;}
}
int dolly_checksum_update(DollyChecksum *s,const void *data,size_t n){
    const unsigned char *p=(const unsigned char *)data;
    size_t i,j;
    if(!s || (!p&&n) || n>UINT64_MAX-s->bytes)return -1;
    for(i=0;i<n;i++){
        s->crc^=p[i];
        for(j=0;j<8;j++)s->crc=(s->crc>>1)^
             ((s->crc&1)?UINT32_C(0xedb88320):0u);
    }
    s->bytes+=n;
    return 0;
}
uint32_t dolly_checksum_final(const DollyChecksum *s){
    return s?(s->crc^UINT32_C(0xffffffff)):0;
}
