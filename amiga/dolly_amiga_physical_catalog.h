#ifndef DOLLY_AMIGA_PHYSICAL_CATALOG_H
#define DOLLY_AMIGA_PHYSICAL_CATALOG_H
/* Bounded immutable snapshot from registered AmigaDOS disk startup
 * records. Never opens drivers, sends I/O or follows DOS pointers
 * after UnLockDosList. A registration is not proof of present media.
 */
#include "dolly_dma_policy.h"
#include <exec/types.h>
#include <dos/dos.h>
#include <stdint.h>
#define DOLLY_PHYS_MAX 16u
#define DOLLY_PHYS_DRIVER_LEN 60u
typedef struct DollyPhysicalEntry {
    char driver[DOLLY_PHYS_DRIVER_LEN];
    ULONG unit;
} DollyPhysicalEntry;
typedef struct DollyPhysicalCatalog {
    unsigned count,truncated,available;
    DollyPhysicalEntry entries[DOLLY_PHYS_MAX];
} DollyPhysicalCatalog;
/* Returns 1 for a unique disk, 2 for the registered RAM handler, 0 otherwise. */
int dolly_physical_from_lock(BPTR lock,DollyPhysicalEntry *device);
void dolly_physical_catalog_snapshot(DollyPhysicalCatalog *out);
int dolly_logical_extent(const char *name,DollyPhysicalEntry *device,
                         uint64_t *start,uint64_t *length,ULONG *sector_bytes,
                         DollyIoHints *hints);
#endif
