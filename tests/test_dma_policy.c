#include "dolly_dma_policy.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
    assert(dolly_dma_check_buffer(0x00100000u,512,0x00fffffeu,2)==DOLLY_DMA_OK);
    assert(dolly_dma_check_buffer(0x00fffe00u,512,0x00fffffeu,2)==DOLLY_DMA_OK);
    assert(dolly_dma_check_buffer(0x00ffff00u,512,0x00fffffeu,2)==DOLLY_DMA_OUTSIDE_MASK);
    assert(dolly_dma_check_buffer(0x01000000u,512,0x00fffffeu,2)==DOLLY_DMA_OUTSIDE_MASK);
    assert(dolly_dma_check_buffer(0x00100001u,512,0x00fffffeu,2)==DOLLY_DMA_MISALIGNED);
    assert(dolly_dma_check_buffer(0x00100002u,512,0x00fffffeu,4)==DOLLY_DMA_MISALIGNED);
    assert(dolly_dma_check_buffer(0x00100000u,512,0,2)==DOLLY_DMA_BAD_PROFILE);
    assert(dolly_dma_check_buffer(0x00100000u,512,0xffffffffu,3)==DOLLY_DMA_BAD_PROFILE);
    assert(dolly_dma_check_buffer(0x00100000u,0,0xffffffffu,2)==DOLLY_DMA_BAD_PROFILE);
    assert(dolly_dma_check_buffer(UINT64_MAX-1,4,0xffffffffu,2)==DOLLY_DMA_ADDRESS_OVERFLOW);
    assert(dolly_dma_check_buffer(UINT64_C(0x100000000),512,0xffffffffu,2)==DOLLY_DMA_OUTSIDE_MASK);
    /* Endpoints may each satisfy a sparse mask even though the interval
     * traverses a forbidden address region in between them. */
    assert(dolly_dma_check_buffer(0x00000ff0u,0x1020u,
                                  0x00002ffeu,2)==DOLLY_DMA_OUTSIDE_MASK);
    /* Deterministic point-wise oracle for randomized small intervals.
     * Every byte address is checked against the mask independently. */
    {
        uint32_t seed=0xd011a600u;
        unsigned i;
        for(i=0;i<6000;i++){
            uint32_t address,bytes,mask,k;
            int valid=1;
            DollyDmaStatus got;
            seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
            address=(seed&4094u);
            seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
            bytes=(seed%128u)+1u;
            seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;
            mask=(seed&0x3ffeu);
            if(!mask)mask=2;
            if(address&~mask)valid=0;
            for(k=0;k<bytes;k++){
                if((address+k)&~(mask|1u))valid=0;
            }
            got=dolly_dma_check_buffer(address,bytes,mask,2);
            assert((got==DOLLY_DMA_OK)==valid);
        }
    }
    puts("DMA profile range/alignment tests passed");
    return 0;
}
