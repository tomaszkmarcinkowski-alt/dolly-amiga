#ifndef DOLLY_DMA_DIAGNOSTICS_H
#define DOLLY_DMA_DIAGNOSTICS_H
/* Ready-to-display DMA explanation: general safe advice + optional
 * documented controller-specific note. Never authorizes transfers. */
#include "dolly_dma_advice.h"
#include "dolly_device_profiles.h"
typedef struct DollyDmaDiagnostic {
    DollyDmaAdvice generic;
    DollyProfileDmaHelp device;
    uint8_t has_documented_device_note;
    uint8_t transfer_authorized; /* deliberately always zero */
} DollyDmaDiagnostic;
/* A controller-specific note requires a previously verified exact-release
 * database and unique exact controller/driver/revision match. The caller
 * must validate the actual file before parsing any records. */
DollyDmaDiagnostic dolly_dma_diagnostic_build(
    DollyDmaStatus problem,DollyDmaAdviceLanguage language,
    const DollyDeviceProfile *profiles,uint32_t profile_count,
    const DollyDeviceIdentity *device,int database_verified);
#endif
