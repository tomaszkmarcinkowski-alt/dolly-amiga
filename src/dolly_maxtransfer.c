#include "dolly_maxtransfer.h"

DollyMaxTransferClass dolly_maxtransfer_classify(uint32_t bytes, int present) {
    if (!present) return DOLLY_MT_NOT_PRESENT;
    if (!bytes) return DOLLY_MT_INVALID_ZERO;
    if (bytes < DOLLY_MAXTRANSFER_REFERENCE) return DOLLY_MT_BELOW_REFERENCE;
    if (bytes == DOLLY_MAXTRANSFER_REFERENCE) return DOLLY_MT_AT_REFERENCE;
    return DOLLY_MT_ABOVE_REFERENCE;
}

const char *dolly_maxtransfer_class_name(DollyMaxTransferClass category) {
    switch (category) {
    case DOLLY_MT_NOT_PRESENT: return "NOT_PRESENT";
    case DOLLY_MT_INVALID_ZERO: return "INVALID_ZERO";
    case DOLLY_MT_BELOW_REFERENCE: return "BELOW_REFERENCE";
    case DOLLY_MT_AT_REFERENCE: return "AT_REFERENCE";
    case DOLLY_MT_ABOVE_REFERENCE: return "ABOVE_REFERENCE";
    default: return "UNKNOWN_CLASS";
    }
}

const char *dolly_maxtransfer_guidance(DollyMaxTransferClass category) {
    switch (category) {
    case DOLLY_MT_NOT_PRESENT:
        return "MaxTransfer not available in this record; inspect active mount settings.";
    case DOLLY_MT_INVALID_ZERO:
        return "Zero MaxTransfer is suspicious; verify RDB and active mount settings.";
    case DOLLY_MT_BELOW_REFERENCE:
        return "A stricter limit is configured; it may be intentional. Do not raise automatically.";
    case DOLLY_MT_AT_REFERENCE:
        return "Matches conservative legacy IDE reference; this does not prove hardware safety.";
    case DOLLY_MT_ABOVE_REFERENCE:
        return "Above conservative 0x1FE00: check driver and medium. Some classic IDE setups risk silent data corruption; 0x1FE00 may be appropriate, but never change automatically.";
    default: return "Unrecognized status.";
    }
}
