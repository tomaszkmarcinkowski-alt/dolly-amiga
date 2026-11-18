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
    case DOLLY_DMA_NO_COMPATIBLE_MEMORY:return "No compatible DMA memory";
    default:return "Unknown DMA status";
    }
}

const char *dolly_dma_user_message(DollyDmaStatus status){
    switch(status){
    case DOLLY_DMA_OK:
        return "DMA buffer checks passed. This does not prove controller compatibility.";
    case DOLLY_DMA_BAD_PROFILE:
        return "DMA configuration is missing or invalid. Device access was stopped. "
               "Check the controller model, DMA address mask and required alignment; "
               "do not guess these values.";
    case DOLLY_DMA_OUTSIDE_MASK:
        return "DMA buffer is outside the controller's permitted address range. "
               "Device access was stopped to prevent an unsafe transfer. "
               "Check the memory type and DMA mask.";
    case DOLLY_DMA_MISALIGNED:
        return "DMA buffer alignment is incompatible with the selected controller. "
               "Device access was stopped. Check the required alignment.";
    case DOLLY_DMA_ADDRESS_OVERFLOW:
        return "DMA buffer address range is invalid or overflows. "
               "Device access was stopped. Verify the controller profile.";
    case DOLLY_DMA_NO_COMPATIBLE_MEMORY:
        return "Cannot allocate memory suitable for the controller DMA requirements. "
               "Device access was stopped. Check free compatible RAM and controller settings.";
    default:
        return "Unknown DMA diagnostic. Device access should remain disabled.";
    }
}

void dolly_io_hints_merge(DollyIoHints *into,DollyIoHints from) {
    if(from.present&DOLLY_IO_MASK)
        into->mask=(into->present&DOLLY_IO_MASK)?into->mask&from.mask:from.mask;
    if(from.present&DOLLY_IO_BUF_MEM)
        into->buffer_memory=(into->present&DOLLY_IO_BUF_MEM)?
                            into->buffer_memory|from.buffer_memory:from.buffer_memory;
    if((from.present&DOLLY_IO_MAX_TRANSFER) &&
       (!(into->present&DOLLY_IO_MAX_TRANSFER) || from.max_transfer<into->max_transfer))
        into->max_transfer=from.max_transfer;
    into->present|=from.present;
}
void dolly_io_hints_override(DollyIoHints *into,DollyIoHints manual) {
    if(manual.present&DOLLY_IO_MASK)into->mask=manual.mask;
    if(manual.present&DOLLY_IO_BUF_MEM)into->buffer_memory=manual.buffer_memory;
    if(manual.present&DOLLY_IO_MAX_TRANSFER)into->max_transfer=manual.max_transfer;
    into->present|=manual.present;
}
int dolly_io_hints_valid(DollyIoHints hints) {
    const uint32_t supported=1u|DOLLY_IO_MEM_CHIP|DOLLY_IO_MEM_FAST|
                             (1u<<8)|DOLLY_IO_MEM_24BIT;
    return (!(hints.present&DOLLY_IO_MASK) || dolly_io_alignment(hints.mask)) &&
           (!(hints.present&DOLLY_IO_BUF_MEM) ||
            (!(hints.buffer_memory&~supported) &&
             (hints.buffer_memory&(DOLLY_IO_MEM_CHIP|DOLLY_IO_MEM_FAST))!=
             (DOLLY_IO_MEM_CHIP|DOLLY_IO_MEM_FAST))) &&
           (!(hints.present&DOLLY_IO_MAX_TRANSFER) || hints.max_transfer!=0);
}
uint32_t dolly_io_alignment(uint32_t mask) {
    uint32_t alignment=2;
    if(!(mask&~1u))return 0;
    while(!(mask&(alignment-1u)) && !(mask&alignment)) {
        if(alignment==4096)return 0;
        alignment<<=1;
    }
    return alignment;
}
