#ifndef DOLLY_SELFCHECK_H
#define DOLLY_SELFCHECK_H
/* Own executable integrity gate: run inside Dolly AFTER process startup,
 * BEFORE enabling any disk operation. Exact build ID + size + CRC32.
 * No OS filesystem dependency; caller opens own executable read-only.
 * The current design embeds the expected CRC32 and version in the binary.
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
    DOLLY_SELF_CRC_MISMATCH,
    DOLLY_SELF_RECORD_MISSING,
    DOLLY_SELF_RECORD_INVALID,
    DOLLY_SELF_RECORD_UNSTAMPED
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
/* Embedded binary record: unique 16-byte magic, 8 ASCII hex CRC32,
 * 16 ASCII release ID bytes (space padded). The checksum treats only
 * the eight ASCII checksum characters as zeroes to avoid recursion.
 * It covers the record's magic/version and ALL remaining binary bytes.
 * A post-link tool populates the version and checksum, no sidecar file. */
#define DOLLY_SELF_MAGIC "DOLLY:CRC32:V1!!"
#define DOLLY_SELF_MAGIC_SIZE 16u
#define DOLLY_SELF_CRC_OFFSET 16u
#define DOLLY_SELF_CRC_SIZE 8u
#define DOLLY_SELF_RELEASE_OFFSET 24u
#define DOLLY_SELF_RELEASE_SIZE 16u
#define DOLLY_SELF_RECORD_SIZE 40u
extern const unsigned char dolly_selfcheck_record[DOLLY_SELF_RECORD_SIZE];
DollySelfCheckReport dolly_selfcheck_embedded_stream(
    DollySelfRead read,void *context,
    const unsigned char loaded_record[DOLLY_SELF_RECORD_SIZE]);
/* Short diagnostic suitable for an early console error. */
const char *dolly_selfcheck_message(DollySelfCheckResult result);
#endif
