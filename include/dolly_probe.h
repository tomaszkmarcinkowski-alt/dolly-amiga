#ifndef DOLLY_PROBE_H
#define DOLLY_PROBE_H
/* Host-only, read-only M0 inspector. Native Amiga I/O is not implemented. */
#include <stdint.h>
#include <stdio.h>

typedef enum DollyImageKind {
    DOLLY_IMAGE_UNKNOWN = 0,
    DOLLY_IMAGE_RAW,
    DOLLY_IMAGE_ADF_DD,
    DOLLY_IMAGE_ADF_HD,
    DOLLY_IMAGE_IFF_DOLY
} DollyImageKind;
typedef enum DollyProbeError {
    DOLLY_PROBE_OK = 0,
    DOLLY_PROBE_IO,
    DOLLY_PROBE_INVALID_IFF,
    DOLLY_PROBE_TOO_SMALL
} DollyProbeError;
typedef struct DollyProbeResult {
    DollyImageKind kind;
    uint64_t file_size;
    uint32_t iff_form_size;
    int iff_size_matches_file;
    int looks_like_iff;
    char note[160];
} DollyProbeResult;
DollyProbeError dolly_probe_stream(FILE *file, DollyProbeResult *out);
const char *dolly_kind_name(DollyImageKind kind);
const char *dolly_probe_error_name(DollyProbeError error);
#endif
