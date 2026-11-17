#ifndef DOLLY_DEVICE_PROFILES_H
#define DOLLY_DEVICE_PROFILES_H
/* Device profile suggestions: deliberately non-authoritative in M0.
 * Loaded plugin metadata cannot by itself verify actual DMA safety. */
#include <stdint.h>
typedef enum DollyProfileMatch {
    DOLLY_PROFILE_NONE=0,
    DOLLY_PROFILE_AMBIGUOUS,
    DOLLY_PROFILE_EXACT
} DollyProfileMatch;
typedef struct DollyDeviceIdentity {
    const char *controller_model;
    const char *driver_name;
    uint32_t driver_version;
    uint32_t hardware_revision;
} DollyDeviceIdentity;
typedef struct DollyDeviceProfile {
    DollyDeviceIdentity identity;
    uint32_t proposed_dma_mask;
    uint32_t proposed_alignment;
    uint32_t proposed_max_transfer;
    uint8_t require_24bit_dma;
    const char *source_reference;
} DollyDeviceProfile;
typedef struct DollyProfileSuggestion {
    DollyProfileMatch match;
    const DollyDeviceProfile *profile; /* only for exact unique match */
    uint32_t candidate_count;
    uint8_t settings_may_be_activated; /* always zero in M0 */
} DollyProfileSuggestion;
/* Pure lookup, no file access, no external writes and no verification. */
DollyProfileSuggestion dolly_device_profile_suggest(
    const DollyDeviceProfile *profiles,uint32_t count,
    const DollyDeviceIdentity *device);
#endif
