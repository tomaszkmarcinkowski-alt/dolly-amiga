#ifndef DOLLY_IFF_H
#define DOLLY_IFF_H
/* Structural IFF 85 / FORM DOLY envelope validation only (no payload decode). */
#include "dolly_block_source.h"
#include <stdint.h>
#include <stddef.h>
typedef enum DollyIffStatus {
 DOLLY_IFF_OK=0, DOLLY_IFF_BAD_ARGUMENT, DOLLY_IFF_TRUNCATED,
 DOLLY_IFF_BAD_FORM, DOLLY_IFF_BAD_SIZE, DOLLY_IFF_BAD_CHUNK,
 DOLLY_IFF_TOO_MANY, DOLLY_IFF_IO_ERROR, DOLLY_IFF_STOPPED
} DollyIffStatus;
typedef struct DollyIffChunk {
 char id[5];
 uint64_t header_offset;
 uint64_t data_offset;
 uint32_t data_size;
} DollyIffChunk;
typedef struct DollyIffSummary {
 uint32_t form_data_bytes;
 uint32_t chunk_count;
 uint64_t total_file_bytes;
} DollyIffSummary;
typedef int (*DollyIffVisitor)(void *ctx,const DollyIffChunk *chunk);
/* Strict root FORM: only DOLY, exact file length, 32-bit nonnegative IFF
 * lengths, 2-byte padded chunk boundaries. Unknown chunk IDs are allowed.
 * At most one small chunk header is in memory. Payload bytes are not parsed.
 */
DollyIffStatus dolly_iff_scan(DollyBlockSource *source,
                             DollyIffVisitor visitor,void *visitor_ctx,
                             DollyIffSummary *out);
const char *dolly_iff_status_name(DollyIffStatus status);
#endif
