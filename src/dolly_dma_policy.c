#include "dolly_dma_policy.h"
#include <stdint.h>
DollyDmaStatus dolly_dma_check_buffer(uint64_t addr,uint64_t bytes,
                                    uint32_t mask,uint32_t alignment){
    uint64_t end,range_mask,difference,low_bits;
    if(!mask||alignment<2||alignment>4096 ||
       (alignment&(alignment-1))!=0 || !bytes)
        return DOLLY_DMA_BAD_PROFILE;
    if(addr>UINT64_MAX-(bytes-1))
        return DOLLY_DMA_ADDRESS_OVERFLOW;
    end=addr+bytes-1;
    if(addr&(alignment-1))return DOLLY_DMA_MISALIGNED;
    if(addr & ~(uint64_t)mask)return DOLLY_DMA_OUTSIDE_MASK;
    /* Mask low bits indicate start-alignment constraints. They must
     * not reject the final odd byte of an otherwise valid transfer. */
    range_mask=(uint64_t)mask | ((uint64_t)alignment-1u);
    /* OR of all addresses in [addr,end]. Checking endpoints alone would
     * miss a forbidden bit crossed inside an irregular memory mask. */
    difference=addr^end;
    low_bits=0;
    while(difference){low_bits=(low_bits<<1)|1u;difference>>=1;}
    if((end|low_bits)&~range_mask)return DOLLY_DMA_OUTSIDE_MASK;
    return DOLLY_DMA_OK;
}
const char *dolly_dma_status_name(DollyDmaStatus s){
    switch(s){
    case DOLLY_DMA_OK:return "OK";
    case DOLLY_DMA_BAD_PROFILE:return "Invalid or missing DMA profile";
    case DOLLY_DMA_OUTSIDE_MASK:return "Buffer outside configured DMA mask";
    case DOLLY_DMA_MISALIGNED:return "DMA buffer misaligned";
    case DOLLY_DMA_ADDRESS_OVERFLOW:return "DMA buffer address overflow";
    default:return "Unknown DMA status";
    }
}
