#ifndef DOLLY_RESTORE_CHECK_H
#define DOLLY_RESTORE_CHECK_H
#include "dolly_block_source.h"
#include <stddef.h>

typedef enum DollyRestoreKind {
    DOLLY_RESTORE_UNKNOWN,
    DOLLY_RESTORE_PARTITION,
    DOLLY_RESTORE_RDB_DISK
} DollyRestoreKind;

typedef struct DollyRestoreExtent {
    DollyBlockSource *blocks;
    uint64_t start,bytes;
    uint32_t sector_bytes;
    DollyRestoreKind expected_kind; /* UNKNOWN for a flat image. */
} DollyRestoreExtent;

typedef enum DollyRestoreDecision {
    DOLLY_RESTORE_MATCH,
    DOLLY_RESTORE_MISMATCH,
    DOLLY_RESTORE_BLOCKED
} DollyRestoreDecision;

typedef enum DollyRestoreSize {
    DOLLY_RESTORE_SIZE_EQUAL,
    DOLLY_RESTORE_SIZE_SMALLER,
    DOLLY_RESTORE_SIZE_LARGER
} DollyRestoreSize;

typedef struct DollyRestoreAssessment {
    DollyRestoreDecision decision;
    DollyRestoreSize size;
    char reason[192];
} DollyRestoreAssessment;

/* Assess read-only spans before authorization. Size relation is reported
 * separately for explicit approval of a bounded sector transfer. This interface transfers no data. */
DollyRestoreAssessment dolly_restore_assess(const DollyRestoreExtent *image,
                                             const DollyRestoreExtent *target);
#endif
