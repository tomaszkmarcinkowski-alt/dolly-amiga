#ifndef DOLLY_RDB_AUDIT_H
#define DOLLY_RDB_AUDIT_H
/* Additional checks performed AFTER an RDB was parsed; no source writes. */
#include "dolly_rdb.h"
#include <stdint.h>

#define DOLLY_RDB_AUDIT_OVERLAPPING_PARTS UINT32_C(0x0001)
#define DOLLY_RDB_AUDIT_HEADER_COLLISION  UINT32_C(0x0002)
#define DOLLY_RDB_AUDIT_PART_COLLISION    UINT32_C(0x0004)
#define DOLLY_RDB_AUDIT_OUTSIDE_CONTAINER UINT32_C(0x0008)
#define DOLLY_RDB_AUDIT_BAD_ARGUMENT      UINT32_C(0x0010)

typedef struct DollyRdbAudit {
    uint32_t flags;
    uint32_t first_partition;
    uint32_t second_partition;
} DollyRdbAudit;

/* Container bounds: absolute start/size in bytes. For whole-disk RDB,
 * start=0 and length=source size. For nested 0x76 RDB, pass the MBR range.
 * Flag indices are zero-based. A zero flags value means no conflicts found
 * among the currently recognized PART/header blocks, NOT a healthy disk. */
DollyRdbAudit dolly_rdb_audit(const DollyRdb *r,
                             uint64_t container_start, uint64_t container_length);
#endif
