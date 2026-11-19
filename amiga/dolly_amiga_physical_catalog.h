#ifndef DOLLY_AMIGA_PHYSICAL_CATALOG_H
#define DOLLY_AMIGA_PHYSICAL_CATALOG_H
/* Read-only snapshot of DOS disk tuples and Exec driver candidates.
 * Unit probing is separate and runs after driver selection. */
#include "dolly_dma_policy.h"
#include <exec/types.h>
#include <dos/dos.h>
#include <stdint.h>
#define DOLLY_PHYS_MAX 16u
#define DOLLY_PHYS_DRIVER_LEN 60u
#define DOLLY_UNIT_PROBE_COUNT 10u
typedef struct DollyPhysicalEntry {
    char driver[DOLLY_PHYS_DRIVER_LEN];
    ULONG unit;
    unsigned char unit_registered;
} DollyPhysicalEntry;
typedef struct DollyPhysicalCatalog {
    unsigned count,truncated,available;
    DollyPhysicalEntry entries[DOLLY_PHYS_MAX];
} DollyPhysicalCatalog;
/* Returns 1 for a unique disk, 2 for the registered RAM handler, 0 otherwise. */
/* A disk handler can have an unknown extent, reported as length zero. */
int dolly_physical_from_lock(BPTR lock,DollyPhysicalEntry *device,
                            uint64_t *start,uint64_t *length);
/* Returns 1 for a registered unit and -1 if enumeration is unavailable. */
int dolly_amiga_unit_registered(const char *driver,ULONG unit);
void dolly_physical_catalog_snapshot(DollyPhysicalCatalog *out);
unsigned dolly_physical_probe_units(const char *driver,ULONG units[DOLLY_UNIT_PROBE_COUNT]);
int dolly_logical_extent(const char *name,DollyPhysicalEntry *device,
                         uint64_t *start,uint64_t *length,ULONG *sector_bytes,
                         DollyIoHints *hints);
#endif
