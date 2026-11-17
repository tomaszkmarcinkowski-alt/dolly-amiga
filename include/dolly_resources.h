#ifndef DOLLY_RESOURCES_H
#define DOLLY_RESOURCES_H
/* Optional file discovery: pure C99, no AmigaDOS dependency or I/O.
 * Caller provides read-only exists callback; missing addons never prevent
 * core startup. No allocation; callback borrows a temporary path string. */
#include "dolly_paths.h"
#include <stddef.h>
#include <stdint.h>
typedef enum DollyResourceKind {
    DOLLY_RESOURCE_GUI=0,
    DOLLY_RESOURCE_FILESYSTEM,
    DOLLY_RESOURCE_DEVICE_DATABASE,
    DOLLY_RESOURCE_CATALOG,
    DOLLY_RESOURCE_HELP,
    DOLLY_RESOURCE_KIND_COUNT
} DollyResourceKind;
typedef struct DollyResourceSpec {
    const char *relative_path; /* within DOLLY:, validated by dolly_paths */
    DollyResourceKind kind;
} DollyResourceSpec;
typedef struct DollyResourceNotice {
    const char *code; /* stable code for logs/translations */
    const char *headline; /* playful but technical info must follow */
    const char *missing_path; /* borrowed: only valid inside callback */
    const char *disabled_feature;
    const char *suggested_action;
} DollyResourceNotice;
/* Probe result: 1 exists/readable, 0 absent, -1 inaccessible/I/O error.
 * Negative outcomes MUST NOT be treated as a successful resource load. */
typedef int (*DollyResourceExists)(const char *logical_path,void *context);
typedef void (*DollyResourceReport)(const DollyResourceNotice *,void *context);
typedef enum DollyResourceScanStatus {
    DOLLY_RESOURCE_SCAN_OK=0,
    DOLLY_RESOURCE_SCAN_INVALID,
    DOLLY_RESOURCE_SCAN_PATH_TOO_LONG
} DollyResourceScanStatus;
typedef struct DollyResourceScanResult {
    uint32_t missing_kinds; /* bit per DollyResourceKind */
    unsigned missing_files;
    unsigned unreadable_files;
    DollyResourceScanStatus status;
} DollyResourceScanResult;
/* All manifest entries are OPTIONAL in this API. Required core assets are
 * validated separately and cannot be silently disabled. A missing plugin
 * disables its feature but returns SCAN_OK. */
DollyResourceScanResult dolly_resources_scan_optional(
    const DollyResourceSpec *items,size_t count,
    DollyResourceExists exists,void *exists_context,
    DollyResourceReport report,void *report_context);
const char *dolly_resource_kind_directory(DollyResourceKind kind);
#endif
