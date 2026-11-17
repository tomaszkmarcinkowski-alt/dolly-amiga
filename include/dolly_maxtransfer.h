#ifndef DOLLY_MAXTRANSFER_H
#define DOLLY_MAXTRANSFER_H

/* Read-only analysis of the filesystem's DosEnvec de_MaxTransfer.
 * 0x1FE00 is a conservative reference for certain legacy IDE drivers,
 * NOT a universal proven limit for every Amiga storage controller. */
#include <stdint.h>

#define DOLLY_MAXTRANSFER_REFERENCE UINT32_C(0x0001FE00)

typedef enum DollyMaxTransferClass {
    DOLLY_MT_NOT_PRESENT = 0,
    DOLLY_MT_INVALID_ZERO,
    DOLLY_MT_BELOW_REFERENCE,
    DOLLY_MT_AT_REFERENCE,
    DOLLY_MT_ABOVE_REFERENCE
} DollyMaxTransferClass;

DollyMaxTransferClass dolly_maxtransfer_classify(uint32_t bytes, int present);
const char *dolly_maxtransfer_class_name(DollyMaxTransferClass category);
const char *dolly_maxtransfer_guidance(DollyMaxTransferClass category);
#endif
