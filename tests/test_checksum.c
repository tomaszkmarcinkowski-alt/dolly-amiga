#include "dolly_checksum.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int main(void){
    DollyChecksum s;
    dolly_checksum_init(&s);
    assert(dolly_checksum_final(&s)==0);
    assert(dolly_checksum_update(&s,"123456789",9)==0);
    assert(dolly_checksum_final(&s)==UINT32_C(0xcbf43926));
    dolly_checksum_init(&s);
    assert(dolly_checksum_update(&s,"1234",4)==0);
    assert(dolly_checksum_update(&s,"56789",5)==0);
    assert(s.bytes==9 && dolly_checksum_final(&s)==UINT32_C(0xcbf43926));
    assert(dolly_checksum_update(&s,NULL,1)==-1);
    assert(dolly_checksum_update(&s,NULL,0)==0);
    assert(dolly_checksum_update(NULL,"x",1)==-1);
    s.bytes=UINT64_MAX;
    assert(dolly_checksum_update(&s,"x",1)==-1);
    puts("Dolly streaming CRC32 accidental-corruption test passed");
    return 0;
}
