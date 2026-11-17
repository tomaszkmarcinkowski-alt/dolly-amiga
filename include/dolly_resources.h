#ifndef DOLLY_RESOURCES_H
#define DOLLY_RESOURCES_H
/* All optional resources require an exact current Dolly release identity.
 * OS-specific probe must inspect the ACTUAL file, not just its filename.
 * C99, no allocation, no filesystem operations in this library. */
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
    const char *relative_path; /* category subdirectory in DOLLY: */
    DollyResourceKind kind;
} DollyResourceSpec;
typedef enum DollyResourceProbeState {
    DOLLY_RESOURCE_ABSENT=0,
    DOLLY_RESOURCE_READABLE,
    DOLLY_RESOURCE_UNREADABLE,
    DOLLY_RESOURCE_UNVERIFIED, /* metadata absent, corrupt or unverifiable */
} DollyResourceProbeState;
typedef struct DollyResourceProbeResult {
    DollyResourceProbeState state;
    const char *release_id;  /* exact Dolly build/release ID; NULL => reject */
    uint32_t format_version; /* validated format ABI/schema, not version alone */
    uint8_t integrity_ok;    /* OS loader validates file + metadata binding */
} DollyResourceProbeResult;
/* Inspect the actual resource via read-only operation. Returned release_id
 * is borrowed and must remain alive until callback returns. On native OS,
 * ensure version metadata is bound to the exact file (hash or embedded
 * header), not merely a plausible sidecar filename. */
typedef DollyResourceProbeResult (*DollyResourceInspect)(
    const char *logical_path,void *context);
typedef struct DollyResourceNotice {
    const char *code;
    const char *headline;
    const char *missing_path; /* historical name: exact affected path */
    const char *disabled_feature;
    const char *suggested_action;
    const char *required_release;
    const char *found_release; /* may be NULL */
} DollyResourceNotice;
typedef void (*DollyResourceReport)(const DollyResourceNotice *,void *context);
typedef enum DollyResourceScanStatus {
    DOLLY_RESOURCE_SCAN_OK=0,
    DOLLY_RESOURCE_SCAN_INVALID,
    DOLLY_RESOURCE_SCAN_PATH_TOO_LONG
} DollyResourceScanStatus;
typedef struct DollyResourceScanResult {
    uint32_t disabled_kinds; /* OR of kinds with unavailable resources */
    unsigned missing_files,unreadable_files,incompatible_files,unverified_files;
    DollyResourceScanStatus status;
} DollyResourceScanResult;
/* required_release must be a nonempty exact current-build identifier.
 * required_format is the specific parser/plugin API version for this kind;
 * the caller must not load any plugin unless BOTH checks pass. Any resource
 * whose version/integrity cannot be proved is rejected. Optional failures
 * do not block the rest of Dolly. */
DollyResourceScanResult dolly_resources_scan_optional(
    const DollyResourceSpec *items,size_t count,
    const char *required_release,uint32_t required_format,
    DollyResourceInspect inspect,void *inspect_context,
    DollyResourceReport report,void *report_context);
const char *dolly_resource_kind_directory(DollyResourceKind kind);
#endif
