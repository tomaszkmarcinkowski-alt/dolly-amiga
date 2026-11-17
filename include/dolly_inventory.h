#ifndef DOLLY_INVENTORY_H
#define DOLLY_INVENTORY_H
/* Sheep Inventory foundation: read-only, bounded and allocation-free.
 * Callbacks consume fields synchronously: string pointers are not owned. */
#include "dolly_partition_map.h"
#include <stdint.h>
typedef enum DollyInventoryValueKind {
    DOLLY_INV_U64 = 0, DOLLY_INV_TEXT
} DollyInventoryValueKind;
typedef enum DollyInventoryAvailability {
    DOLLY_INV_PRESENT = 0, DOLLY_INV_UNAVAILABLE,
    DOLLY_INV_NOT_QUERIED, DOLLY_INV_UNSUPPORTED, DOLLY_INV_INCONSISTENT
} DollyInventoryAvailability;
typedef enum DollyInventoryOrigin {
    DOLLY_INV_RDB_PART = 0, DOLLY_INV_DERIVED, DOLLY_INV_MBR,
    DOLLY_INV_RDB_HEADER
} DollyInventoryOrigin;
/* Evidence describes what a value MEANS, independently of which object supplied it. */
typedef enum DollyInventoryEvidence {
    DOLLY_EVIDENCE_DECLARED = 0,  /* Recorded metadata; not verified hardware */
    DOLLY_EVIDENCE_OBSERVED,      /* Direct device/runtime query */
    DOLLY_EVIDENCE_DERIVED,       /* Calculated from other fields */
    DOLLY_EVIDENCE_ESTIMATED      /* An approximation, not a measurement */
} DollyInventoryEvidence;
typedef struct DollyInventoryField {
    const char *key;
    const char *label;
    const char *unit;
    DollyInventoryValueKind kind;
    DollyInventoryAvailability availability;
    DollyInventoryOrigin origin;
    DollyInventoryEvidence evidence;
    uint64_t number;
    const char *text;
} DollyInventoryField;
/* Return nonzero from callback to stop enumeration early. A zero return
 * means every field was delivered. No scans or I/O occur in this function. */
typedef int (*DollyInventoryEmit)(const DollyInventoryField *field, void *user);
int dolly_inventory_partition(const DollyPartition *partition,
                              DollyInventoryEmit emit, void *user);
int dolly_inventory_mbr_partition(const DollyMbrPartition *partition,
                                  DollyInventoryEmit emit, void *user);
int dolly_inventory_rdb_header(const DollyRdb *rdb,
                               DollyInventoryEmit emit, void *user);
const char *dolly_inventory_availability_name(DollyInventoryAvailability availability);
const char *dolly_inventory_evidence_name(DollyInventoryEvidence evidence);
#endif
