#ifndef DOLLY_AMIGA_DEVICE_QUERY_H
#define DOLLY_AMIGA_DEVICE_QUERY_H
#include <exec/types.h>
#include <stdint.h>
/* Called ONLY by user pressing Query. Never by opening a picker,
 * changing its mode, choosing a list entry or resizing a window.
 */
typedef struct DollyDeviceQueryInfo {
    uint64_t capacity_bytes;
    uint32_t sector_bytes;
    unsigned char capacity_valid;
    unsigned char identity_valid;
} DollyDeviceQueryInfo;
int dolly_amiga_device_query_ex(const char *driver,ULONG unit,
                               char *name,unsigned name_capacity,
                               char *size,unsigned size_capacity,
                               DollyDeviceQueryInfo *info);
int dolly_amiga_device_query(const char *driver,ULONG unit,
                            char *name,unsigned name_capacity,
                            char *size,unsigned size_capacity);
#endif
