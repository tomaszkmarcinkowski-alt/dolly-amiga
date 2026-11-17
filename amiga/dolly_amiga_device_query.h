#ifndef DOLLY_AMIGA_DEVICE_QUERY_H
#define DOLLY_AMIGA_DEVICE_QUERY_H
#include <exec/types.h>
#include <stdint.h>
/* Populated only by an explicit user Query. Capacity is independent
 * of SCSI INQUIRY; no changes to the disk are made.
 */
typedef struct DollyMediaGeometry {
    uint64_t block_count;
    ULONG block_bytes;
    unsigned char capacity_valid;
} DollyMediaGeometry;
int dolly_amiga_device_query(const char *driver,ULONG unit,
                            char *name,unsigned name_capacity,
                            char *size,unsigned size_capacity,
                            DollyMediaGeometry *geometry);
#endif
