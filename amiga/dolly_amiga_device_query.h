#ifndef DOLLY_AMIGA_DEVICE_QUERY_H
#define DOLLY_AMIGA_DEVICE_QUERY_H
#include <exec/types.h>
#include <stdint.h>
/* Populated only by an explicit user Query. Capacity is independent
 * of SCSI INQUIRY; no changes to the disk are made.
 */
typedef enum DollyGeometrySource {
    DOLLY_GEOMETRY_UNKNOWN=0,
    DOLLY_GEOMETRY_TRACKDISK=1,
    DOLLY_GEOMETRY_SCSI_CAPACITY10=2
} DollyGeometrySource;
typedef struct DollyMediaGeometry {
    uint64_t block_count;
    ULONG block_bytes;
    unsigned char capacity_valid;
    unsigned char capacity_source;
} DollyMediaGeometry;
int dolly_amiga_device_query(const char *driver,ULONG unit,
                            char *name,unsigned name_capacity,
                            char *size,unsigned size_capacity,
                            DollyMediaGeometry *geometry);
#endif
