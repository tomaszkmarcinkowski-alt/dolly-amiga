#ifndef DOLLY_PROFILE_CONFIG_H
#define DOLLY_PROFILE_CONFIG_H
/* Non-destructive Dolly-local proposal. No OS/RDB writes; no DMA permission. */
#include "dolly_device_profiles.h"
#include <stdint.h>
typedef struct DollyLocalIoSettings {
    uint32_t block_bytes;
    uint32_t transfer_limit_bytes;
    uint32_t retry_limit;
} DollyLocalIoSettings;
typedef enum DollyConfigResult {
    DOLLY_CONFIG_NO_CHANGE=0,
    DOLLY_CONFIG_PROPOSAL_READY,
    DOLLY_CONFIG_REJECTED
} DollyConfigResult;
typedef struct DollyConfigProposal {
    DollyConfigResult status;
    DollyLocalIoSettings before,after;
    const char *source_reference;
    uint8_t requires_confirmation;
    uint8_t has_dma_authorization; /* always zero */
} DollyConfigProposal;
DollyConfigProposal dolly_profile_prepare_config(
    const DollyDeviceProfile *profiles,uint32_t count,
    const DollyDeviceIdentity *device,int database_verified,
    DollyLocalIoSettings current);
#endif
