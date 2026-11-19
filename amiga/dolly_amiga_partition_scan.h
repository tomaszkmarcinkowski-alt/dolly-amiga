#ifndef DOLLY_AMIGA_PARTITION_SCAN_H
#define DOLLY_AMIGA_PARTITION_SCAN_H
#include "dolly_amiga_device_query.h"
#include "dolly_dma_policy.h"
#include <dos/dos.h>
#define DOLLY_PART_MAX 16u
typedef struct DollyNativePartition {
    char label[40];     /* RDB drive name, or explicit MBR entry */
    char summary[160];  /* formatted from validated on-disk fields */
    uint64_t start_bytes,length_bytes;
    ULONG descriptor_block;
    ULONG low_cyl,high_cyl,fs_block_bytes,sectors_per_block,dos_type;
    ULONG max_transfer,dma_mask;
    DollyIoHints io_hints;
    int32_t boot_priority;
    unsigned char dos_type_present,boot_priority_present;
    unsigned char max_transfer_present,dma_mask_present;
    UBYTE mbr_type;
    UBYTE mbr_slot;
    UBYTE mbr_active;
    UBYTE mbr_container;
    unsigned char is_rdb,outside_media;
    unsigned char rdb_bootable,rdb_automount;
} DollyNativePartition;
typedef enum DollyNativePartitionFormat {
    DOLLY_NATIVE_PARTITION_NONE,
    DOLLY_NATIVE_PARTITION_RDB,
    DOLLY_NATIVE_PARTITION_MBR
} DollyNativePartitionFormat;
typedef struct DollyNativePartitionMap {
    unsigned count;
    DollyNativePartitionFormat format;
    ULONG rdb_header_block;
    ULONG rdb_backup_blocks; /* raw prefix length in 512-byte blocks */
    unsigned char mbr_backup_available,rdb_capacity_mismatch,no_partition_table;
    char status[96];
    DollyNativePartition parts[DOLLY_PART_MAX];
} DollyNativePartitionMap;
/* Called ONLY after Query verified the specific driver/unit and geometry.
 * Reads at most 16 RDB discovery sectors and a bounded chain of 16
 * PART blocks, or an MBR boot sector; never writes a disk.
 * A structurally valid RDB can include flagged, unusable extents beyond
 * the reported capacity. Callers must check outside_media before use.
 */
int dolly_amiga_partition_scan(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionMap *out);
/* Read-only inventory may include container entries, without walking them. */
int dolly_amiga_inventory_scan(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionMap *out);
/* Revalidate the expected table. Export the RDB prefix or the MBR sector. */
int dolly_amiga_partition_backup(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionFormat format,
          BPTR file);
#define DOLLY_RDB_RESTORE_MAX_BLOCKS 128u
struct Window;
/* A successful recovery sink publishes a durable copy before writes begin. */
typedef int (*DollyRdbRecoverySink)(void *context,const void *data,ULONG bytes);
/* Returns NULL on success, or a literal diagnostic on failure. */
const char *dolly_amiga_restore_rdb(struct Window *parent,const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,ULONG backup_blocks,BPTR backup,
          DollyRdbRecoverySink save_recovery,void *context);
#endif
