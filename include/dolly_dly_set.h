#ifndef DOLLY_DLY_SET_H
#define DOLLY_DLY_SET_H
#include <stdint.h>

/* MSET immediately follows GEOM in a multipart FORM. */
#define DOLLY_DLY_GEOM_END 60u
#define DOLLY_DLY_MSET_BYTES 40u
#define DOLLY_DLY_NUMBERED_MSET_BYTES 52u

typedef struct DollyDlySetInfo {
    uint64_t first_lba,sectors;
    uint32_t sector_bytes,parts,sectors_per_part;
    uint32_t part; /* One-based. Zero denotes the legacy manifest. */
    uint64_t set_id;
} DollyDlySetInfo;
#endif
