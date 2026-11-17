#ifndef DOLLY_PARTITION_MAP_H
#define DOLLY_PARTITION_MAP_H
#include "dolly_rdb.h"
#include <stdint.h>
#define DOLLY_MBR_MAX 4u
typedef enum DollyMapStatus {
 DOLLY_MAP_OK=0, DOLLY_MAP_NO_TABLE, DOLLY_MAP_BAD_ARGUMENT,
 DOLLY_MAP_READ_ERROR, DOLLY_MAP_OUT_OF_RANGE, DOLLY_MAP_OVERLAP,
 DOLLY_MAP_UNSUPPORTED, DOLLY_MAP_NESTED_RDB_ERROR
} DollyMapStatus;
typedef struct DollyMbrPartition {
 uint8_t type,bootable;
 uint32_t first_lba,sector_count;
 uint64_t start_bytes,length_bytes;
 uint8_t contains_rdb;
 DollyRdb nested_rdb;
} DollyMbrPartition;
typedef struct DollyPartitionMap {
 int has_mbr;
 unsigned partition_count;
 DollyMbrPartition primary[DOLLY_MBR_MAX];
 uint8_t has_whole_disk_rdb;
 DollyRdb whole_disk_rdb;
} DollyPartitionMap;
/* Read-only primary MBR scan; LBA units equal logical device block size.
 * RDB pointers are interpreted relative to the containing partition.
 * GPT, EBR and shared-offset hybrid layouts remain unsupported. */
DollyMapStatus dolly_partition_map_scan(DollyBlockSource *source,
 uint32_t device_block_bytes,DollyPartitionMap *out);
const char *dolly_map_status_name(DollyMapStatus status);
#endif
