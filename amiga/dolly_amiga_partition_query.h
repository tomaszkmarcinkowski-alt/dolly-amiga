#ifndef DOLLY_AMIGA_PARTITION_QUERY_H
#define DOLLY_AMIGA_PARTITION_QUERY_H
#include <exec/types.h>
#include <stdint.h>
#define DOLLY_GUI_PARTITIONS 16u
typedef struct DollyGuiPartition {
    char label[48];
    char details[160];
    uint64_t start_bytes,length_bytes;
} DollyGuiPartition;
typedef struct DollyGuiPartitionResult {
    unsigned count;
    unsigned valid;
    unsigned truncated;
    char status[80];
    DollyGuiPartition entries[DOLLY_GUI_PARTITIONS];
} DollyGuiPartitionResult;
/* Only called inside a user-initiated Query; never automatically.
 * Only a 512-byte sector backend is supported at this milestone.
 * RDB and MBR are interpreted by the portable dollymap parser.
 */
int dolly_amiga_partition_query(const char *driver,ULONG unit,
                                uint64_t capacity,uint32_t sector_bytes,
                                DollyGuiPartitionResult *out);
#endif
