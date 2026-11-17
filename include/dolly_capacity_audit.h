#ifndef DOLLY_CAPACITY_AUDIT_H
#define DOLLY_CAPACITY_AUDIT_H
/* Read-only comparison of RDB geometry with independent media/container
 * size. Does NOT certify a tail as writable or perform geometry repair. */
#include "dolly_rdb.h"
#include <stdint.h>
#define DOLLY_CAPACITY_GEOMETRY_SMALLER UINT32_C(0x0001)
#define DOLLY_CAPACITY_GEOMETRY_LARGER  UINT32_C(0x0002)
#define DOLLY_CAPACITY_PART_OUTSIDE     UINT32_C(0x0004)
#define DOLLY_CAPACITY_META_OUTSIDE     UINT32_C(0x0008)
#define DOLLY_CAPACITY_GEOMETRY_UNKNOWN UINT32_C(0x0010)
#define DOLLY_CAPACITY_OVERFLOW         UINT32_C(0x0020)
#define DOLLY_CAPACITY_INVALID          UINT32_C(0x0040)
#define DOLLY_CAPACITY_PART_PAST_GEOM   UINT32_C(0x0080)
typedef struct DollyCapacityAudit {
    uint32_t flags;
    uint64_t observed_container_bytes;
    uint64_t declared_geometry_bytes; /* 0 when unknown/overflow */
    uint64_t extra_bytes;             /* capacity beyond RDB geometry */
    uint64_t shortfall_bytes;         /* RDB geometry beyond capacity */
    uint32_t first_outside_partition; /* index, valid iff PART_OUTSIDE */
} DollyCapacityAudit;
/* Container start/length are independently observed bytes from device or
 * MBR partition 0x76, not claimed by on-disk RDB. This is intentionally
 * independent of any host geometry guess or mounted filesystem. */
DollyCapacityAudit dolly_capacity_audit(const DollyRdb *r,
                                         uint64_t container_start,
                                         uint64_t container_bytes);
#endif
