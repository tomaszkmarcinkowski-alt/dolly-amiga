#include "dolly_checksum.h"
#include <stdint.h>
void dolly_checksum_init(DollyChecksum *s){
    if(s){s->crc=UINT32_C(0xffffffff);s->bytes=0;}
}
int dolly_checksum_update(DollyChecksum *s,const void *data,size_t n){
    const unsigned char *p=(const unsigned char *)data;
    /* Four reflected CRC bits per lookup, using only 64 bytes on 68000. */
    static const uint32_t nibble[16]={
        UINT32_C(0x00000000), UINT32_C(0x1db71064),
        UINT32_C(0x3b6e20c8), UINT32_C(0x26d930ac),
        UINT32_C(0x76dc4190), UINT32_C(0x6b6b51f4),
        UINT32_C(0x4db26158), UINT32_C(0x5005713c),
        UINT32_C(0xedb88320), UINT32_C(0xf00f9344),
        UINT32_C(0xd6d6a3e8), UINT32_C(0xcb61b38c),
        UINT32_C(0x9b64c2b0), UINT32_C(0x86d3d2d4),
        UINT32_C(0xa00ae278), UINT32_C(0xbdbdf21c),
    };
    size_t i;
    if(!s || (!p&&n) || n>UINT64_MAX-s->bytes)return -1;
    for(i=0;i<n;i++){
        s->crc^=p[i];
        s->crc=(s->crc>>4)^nibble[s->crc&15u];
        s->crc=(s->crc>>4)^nibble[s->crc&15u];
    }
    s->bytes+=n;
    return 0;
}
uint32_t dolly_checksum_final(const DollyChecksum *s){
    return s?(s->crc^UINT32_C(0xffffffff)):0;
}
