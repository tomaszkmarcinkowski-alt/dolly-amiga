#ifndef DOLLY_AMIGA_PARTITION_SCAN_H
#define DOLLY_AMIGA_PARTITION_SCAN_H
#include "dolly_amiga_device_query.h"
#define DOLLY_PART_MAX 16u
typedef struct DollyNativePartition {
    char label[40];     /* RDB drive name, or explicit MBR entry */
    char summary[160];  /* formatted from validated on-disk fields */
    uint64_t start_bytes,length_bytes;
    ULONG low_cyl,high_cyl,fs_block_bytes,sectors_per_block,dos_type;
    UBYTE mbr_type;
    unsigned char is_rdb;
} DollyNativePartition;
typedef struct DollyNativePartitionMap {
    unsigned count;
    char status[96];
    DollyNativePartition parts[DOLLY_PART_MAX];
} DollyNativePartitionMap;
/* Called ONLY after Query verified the specific driver/unit and geometry.
 * Reads at most 16 RDB discovery sectors and a bounded chain of 16
 * PART blocks, or an MBR boot sector; never writes a disk.
 * Returns 1 only for a fully validated, supported map.
 */
int dolly_amiga_partition_scan(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionMap *out);
#endif
