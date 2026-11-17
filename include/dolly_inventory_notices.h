#ifndef DOLLY_INVENTORY_NOTICES_H
#define DOLLY_INVENTORY_NOTICES_H
#include "dolly_rdb.h"
#include <stdint.h>
/* Synchronous, allocation-free advisory feed; NEVER changes disk metadata. */
typedef enum DollyInventoryNoticeSeverity {
    DOLLY_NOTICE_INFO=0, DOLLY_NOTICE_WARNING, DOLLY_NOTICE_ERROR
} DollyInventoryNoticeSeverity;
typedef struct DollyInventoryNotice {
    const char *code;      /* stable nonlocalized identifier */
    const char *message;   /* preliminary English human-readable text */
    const char *suggested_action; /* safe next check, NULL only if none is known */
    DollyInventoryNoticeSeverity severity;
} DollyInventoryNotice;
typedef int (*DollyInventoryNoticeEmit)(const DollyInventoryNotice *,void *user);
/* Returns zero on success; nonzero callback value stops enumeration. */
int dolly_inventory_partition_notices(const DollyPartition *partition,
                                      DollyInventoryNoticeEmit emit,void *user);
/* Audit all recognized PART ranges against each other and RDB/PART metadata.
 * Container bounds are bytes (the host image size for whole-disk RDB). */
int dolly_inventory_rdb_notices(const DollyRdb *rdb,
                               uint64_t container_base,uint64_t container_length,
                               DollyInventoryNoticeEmit emit,void *user);
const char *dolly_notice_severity_name(DollyInventoryNoticeSeverity severity);
#endif
