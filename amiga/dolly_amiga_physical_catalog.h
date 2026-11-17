#ifndef DOLLY_AMIGA_PHYSICAL_CATALOG_H
#define DOLLY_AMIGA_PHYSICAL_CATALOG_H
/* Bounded immutable snapshot from registered AmigaDOS disk startup
 * records. Never opens drivers, sends I/O or follows DOS pointers
 * after UnLockDosList. A registration is not proof of present media.
 */
#include <exec/types.h>
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
void dolly_physical_catalog_snapshot(DollyPhysicalCatalog *out);
#endif
