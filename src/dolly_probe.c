#define _POSIX_C_SOURCE 200809L
#include "dolly_probe.h"
#include <string.h>
#include <sys/types.h>
static uint32_t be32(const unsigned char *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}
const char *dolly_kind_name(DollyImageKind kind) {
    switch(kind) {
    case DOLLY_IMAGE_RAW: return "RAW / HDF candidate";
    case DOLLY_IMAGE_ADF_DD: return "ADF 880 KiB (size-based candidate)";
    case DOLLY_IMAGE_ADF_HD: return "ADF 1760 KiB (size-based candidate)";
    case DOLLY_IMAGE_IFF_DOLY: return "IFF FORM DOLY";
    default: return "Unknown";
    }
}
const char *dolly_probe_error_name(DollyProbeError error) {
    switch(error) {
    case DOLLY_PROBE_OK: return "OK";
    case DOLLY_PROBE_IO: return "File I/O error";
    case DOLLY_PROBE_INVALID_IFF: return "Invalid/truncated IFF FORM header";
    case DOLLY_PROBE_TOO_SMALL: return "Empty input";
    default: return "Unknown probe error";
    }
}
DollyProbeError dolly_probe_stream(FILE *file, DollyProbeResult *out) {
    unsigned char header[12];
    size_t got;
    off_t size;
    uint64_t declared_total;
    if (!file || !out) return DOLLY_PROBE_IO;
    memset(out, 0, sizeof(*out));
    if (fseeko(file, 0, SEEK_END) != 0) return DOLLY_PROBE_IO;
    size = ftello(file);
    if (size < 0 || fseeko(file, 0, SEEK_SET) != 0) return DOLLY_PROBE_IO;
    out->file_size = (uint64_t)size;
    if (size == 0) return DOLLY_PROBE_TOO_SMALL;
    got = fread(header, 1, sizeof(header), file);
    if (ferror(file)) return DOLLY_PROBE_IO;
    if (got >= 4 && memcmp(header, "FORM", 4) == 0) {
        out->looks_like_iff = 1;
        if (got < 12) return DOLLY_PROBE_INVALID_IFF;
        out->iff_form_size = be32(header + 4);
        if (out->iff_form_size < 4 || out->iff_form_size >= UINT32_C(0x80000000))
            return DOLLY_PROBE_INVALID_IFF;
        declared_total = (uint64_t)out->iff_form_size + 8;
        out->iff_size_matches_file = declared_total == out->file_size;
        if (memcmp(header + 8, "DOLY", 4) == 0) {
            out->kind = DOLLY_IMAGE_IFF_DOLY;
            if (!out->iff_size_matches_file) {
                strcpy(out->note, "FORM length differs from file length; validate before use");
                return DOLLY_PROBE_INVALID_IFF;
            }
            strcpy(out->note, "Envelope recognized; internal chunk format not implemented");
            return DOLLY_PROBE_OK;
        }
        strcpy(out->note, "Other IFF FORM; not a DOLY disk image");
        return DOLLY_PROBE_OK;
    }
    if (out->file_size == UINT64_C(901120)) {
        out->kind = DOLLY_IMAGE_ADF_DD;
        strcpy(out->note, "Size fits standard DD ADF; content not validated");
    } else if (out->file_size == UINT64_C(1802240)) {
        out->kind = DOLLY_IMAGE_ADF_HD;
        strcpy(out->note, "Size fits standard HD ADF; content not validated");
    } else {
        out->kind = DOLLY_IMAGE_RAW;
        strcpy(out->note, "RAW image candidate; geometry and filesystem unknown");
    }
    return DOLLY_PROBE_OK;
}
