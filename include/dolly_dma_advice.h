#ifndef DOLLY_DMA_ADVICE_H
#define DOLLY_DMA_ADVICE_H
/* Structured, read-only DMA troubleshooting; static strings, no allocation.
 * ASCII Polish fallback is deliberate for pre-Unicode AmigaOS consoles.
 * A future locale catalog supplies correctly encoded native PL strings. */
#include "dolly_dma_policy.h"
typedef enum DollyDmaAdviceLanguage {
    DOLLY_DMA_ADVICE_EN = 0,
    DOLLY_DMA_ADVICE_PL_ASCII = 1
} DollyDmaAdviceLanguage;
typedef struct DollyDmaAdvice {
    const char *code;           /* stable, untranslated log identifier */
    const char *title;
    const char *reason;
    const char *action[3];      /* NULL if unused; safe diagnostic steps */
    unsigned action_count;
    int operation_blocked;      /* zero only for DOLLY_DMA_OK */
} DollyDmaAdvice;
/* Returned pointers are static, not owned. Bad enums fail closed. */
DollyDmaAdvice dolly_dma_get_advice(DollyDmaStatus status,
                                   DollyDmaAdviceLanguage language);
#endif
