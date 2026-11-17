#ifndef DOLLY_SELFCHECK_H
#define DOLLY_SELFCHECK_H
/* Own executable integrity gate: run inside Dolly AFTER process startup,
 * BEFORE enabling any disk operation. Exact build ID + size + CRC32.
 * No OS filesystem dependency; caller opens own executable read-only.
 * Expected values come from a separately packaged release manifest.
 * CRC32 detects accidental corruption, not malicious replacement.
 */
#include <stdint.h>
#include <stddef.h>
typedef enum DollySelfCheckResult {
    DOLLY_SELF_OK=0,
    DOLLY_SELF_INVALID,
    DOLLY_SELF_OPEN_FAILED,
    DOLLY_SELF_READ_FAILED,
    DOLLY_SELF_VERSION_MISMATCH,
    DOLLY_SELF_SIZE_MISMATCH,
    DOLLY_SELF_CRC_MISMATCH
} DollySelfCheckResult;
/* Return >0 for bytes read, 0 for EOF, <0 for I/O error.
 * The read function may provide a smaller chunk than requested. */
typedef long (*DollySelfRead)(void *context,void *buffer,size_t capacity);
typedef struct DollySelfCheckExpected {
    const char *running_release;
    const char *manifest_release;
    uint64_t size;
    uint32_t crc32;
} DollySelfCheckExpected;
typedef struct DollySelfCheckReport {
    DollySelfCheckResult result;
    uint64_t actual_size;
    uint32_t actual_crc32;
} DollySelfCheckReport;
DollySelfCheckReport dolly_selfcheck_stream(
    DollySelfRead read,void *context,DollySelfCheckExpected expected);
/* Short diagnostic suitable for an early console error. */
const char *dolly_selfcheck_message(DollySelfCheckResult result);
#endif
