#ifndef DOLLY_MEDIA_GEOMETRY_H
#define DOLLY_MEDIA_GEOMETRY_H
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
    uint32_t block_bytes;
    unsigned char capacity_valid;
    unsigned char capacity_source;
    uint32_t cylinders,heads,sectors_per_track;
} DollyMediaGeometry;
#endif
