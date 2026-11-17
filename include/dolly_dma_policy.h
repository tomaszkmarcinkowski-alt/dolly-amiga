#ifndef DOLLY_DMA_POLICY_H
#define DOLLY_DMA_POLICY_H
/* AmigaOS-style DMA aperture/alignment checks. Pure C99; no I/O. */
#include <stdint.h>
#include <stddef.h>
typedef enum DollyDmaStatus {
    DOLLY_DMA_OK=0,
    DOLLY_DMA_BAD_PROFILE,
    DOLLY_DMA_OUTSIDE_MASK,
    DOLLY_DMA_MISALIGNED,
    DOLLY_DMA_ADDRESS_OVERFLOW,
    DOLLY_DMA_NO_COMPATIBLE_MEMORY
} DollyDmaStatus;
/* mask: permitted start-address bits (e.g. 0x00fffffe);
 * alignment: a power of two, at least 2. A buffer spanning forbidden
 * high address bits is rejected. Not a guarantee of bus reachability. */
DollyDmaStatus dolly_dma_check_buffer(uint64_t address,uint64_t bytes,
                                    uint32_t mask,uint32_t alignment);
const char *dolly_dma_status_name(DollyDmaStatus status);
/* User-facing diagnostic (7-bit English fallback for classic AmigaOS).
 * The CLI/GadTools layer is responsible for displaying it and translating
 * through the locale catalog when available. No claim of disk damage. */
const char *dolly_dma_user_message(DollyDmaStatus status);
#endif
