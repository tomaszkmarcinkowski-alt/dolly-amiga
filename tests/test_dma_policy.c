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
    puts("DMA profile range/alignment tests passed");
    return 0;
}
