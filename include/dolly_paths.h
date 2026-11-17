#ifndef DOLLY_PATHS_H
#define DOLLY_PATHS_H
/* Dolly internal resources: one stable, classic AmigaDOS root assign.
 * This layer never creates Assigns or opens files. C99 / 68000 portable. */
#include <stddef.h>
typedef enum DollyPathStatus {
    DOLLY_PATH_OK=0,
    DOLLY_PATH_BAD_ARGUMENT,
    DOLLY_PATH_INVALID_RELATIVE,
    DOLLY_PATH_TOO_LONG
} DollyPathStatus;
/* Relative paths are within DOLLY: and may use '/' subdirectories.
 * Reject absolute paths, colon, empty components, . and .., and control
 * characters. out is NUL-terminated on success and empty on failure. */
DollyPathStatus dolly_path_resource(const char *relative,char *out,
                                    size_t capacity);
const char *dolly_path_status_name(DollyPathStatus status);
#define DOLLY_RESOURCE_ROOT "DOLLY:"
#endif
