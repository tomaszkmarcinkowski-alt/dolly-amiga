#include "dolly_maxtransfer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    assert(DOLLY_MAXTRANSFER_REFERENCE == 130560u);
    assert(dolly_maxtransfer_classify(0,0)==DOLLY_MT_NOT_PRESENT);
    assert(dolly_maxtransfer_classify(0x7fffffffu,0)==DOLLY_MT_NOT_PRESENT);
    assert(dolly_maxtransfer_classify(0,1)==DOLLY_MT_INVALID_ZERO);
    assert(dolly_maxtransfer_classify(0xfe00u,1)==DOLLY_MT_BELOW_REFERENCE);
    assert(dolly_maxtransfer_classify(0x1fe00u,1)==DOLLY_MT_AT_REFERENCE);
    assert(dolly_maxtransfer_classify(0x7fffffffu,1)==DOLLY_MT_ABOVE_REFERENCE);
    assert(dolly_maxtransfer_classify(0xffffffffu,1)==DOLLY_MT_ABOVE_REFERENCE);
    assert(strstr(dolly_maxtransfer_guidance(DOLLY_MT_ABOVE_REFERENCE),"0x1FE00")!=NULL);
    puts("MaxTransfer classification passed");
    return 0;
}
