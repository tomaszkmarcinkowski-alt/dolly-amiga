#ifndef DOLLY_SELFCHECK_H
#define DOLLY_SELFCHECK_H
/* In-process executable CRC32 validation, no separate manifest/launcher.
 * Must run after process startup, BEFORE access to physical devices.
 * A post-link tool stamps the checksum and build version directly into the
 * executable. The CRC calculation substitutes ASCII "00000000" for the
 * eight checksum characters, avoiding a self-referential CRC.
 * CRC32 is for accidental corruption detection, not authenticity.
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
/* Return 1..capacity on success, 0 at EOF and negative on I/O failure. */
typedef long (*DollySelfRead)(void *context,void *buffer,size_t capacity);
typedef struct DollySelfCheckReport {
    DollySelfCheckResult result;
    uint64_t actual_size;
    uint32_t actual_crc32;
} DollySelfCheckReport;
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
const char *dolly_selfcheck_message(DollySelfCheckResult result);
#endif
