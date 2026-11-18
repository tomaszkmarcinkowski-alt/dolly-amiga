#ifndef DOLLY_DMA_POLICY_H
#define DOLLY_DMA_POLICY_H
/* AmigaOS-style DMA aperture/alignment checks. Pure C99; no I/O. */
#include <stdint.h>
#include <stddef.h>
enum {
    DOLLY_IO_MASK=1u, DOLLY_IO_BUF_MEM=2u, DOLLY_IO_MAX_TRANSFER=4u,
    DOLLY_IO_ALL=7u
};
#define DOLLY_IO_MEM_CHIP (1u<<1)
#define DOLLY_IO_MEM_FAST (1u<<2)
#define DOLLY_IO_MEM_24BIT (1u<<9)

typedef struct DollyIoHints {
    uint32_t mask,buffer_memory,max_transfer;
    unsigned present;
} DollyIoHints;

/* Accumulate the most restrictive declarations for a shared unit. */
void dolly_io_hints_merge(DollyIoHints *into,DollyIoHints from);
/* Manual values replace only the specified automatic fields. */
void dolly_io_hints_override(DollyIoHints *into,DollyIoHints manual);
int dolly_io_hints_valid(DollyIoHints hints);
uint32_t dolly_io_alignment(uint32_t mask);
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
