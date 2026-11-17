#ifndef DOLLY_RDB_H
#define DOLLY_RDB_H
/* Host-side, read-only Amiga RDB inspection. No disk writes. */
#include "dolly_block_source.h"
#include <stdint.h>
#define DOLLY_RDB_MAX_PARTITIONS 128u
#define DOLLY_RDB_END UINT32_C(0xffffffff)
typedef enum DollyRdbStatus {
 DOLLY_RDB_OK=0, DOLLY_RDB_NOT_FOUND, DOLLY_RDB_BAD_ARGUMENT,
 DOLLY_RDB_BAD_BLOCK, DOLLY_RDB_BAD_CHECKSUM, DOLLY_RDB_OUT_OF_RANGE,
 DOLLY_RDB_CYCLE, DOLLY_RDB_TOO_MANY, DOLLY_RDB_READ_ERROR
} DollyRdbStatus;
typedef struct DollyPartition {
 uint32_t descriptor_block;
 uint32_t low_cylinder, high_cylinder, dos_type;
 uint32_t max_transfer_bytes; /* DosEnvec de_MaxTransfer, not raw I/O limit */
 uint8_t max_transfer_present;
 uint64_t start_bytes, length_bytes;
 char name[32];
} DollyPartition;
typedef struct DollyRdb {
 uint64_t base_offset;
 uint32_t block_bytes, header_block, partition_head;
 uint32_t cylinders, heads, sectors;
 unsigned partition_count;
 DollyPartition partitions[DOLLY_RDB_MAX_PARTITIONS];
} DollyRdb;
/* Bounded scan [0,scan_blocks) relative to base_offset.
 * block_bytes is the device block size and RDB block-pointer unit.
 * All coordinates require checksum and bounds validation.
 */
DollyRdbStatus dolly_rdb_scan(DollyBlockSource *source,uint64_t base_offset,
 uint32_t block_bytes,uint32_t scan_blocks,DollyRdb *out);
const char *dolly_rdb_status_name(DollyRdbStatus status);
#endif
