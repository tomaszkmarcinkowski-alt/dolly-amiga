#ifndef DOLLY_AMIGA_TRANSFER_H
#define DOLLY_AMIGA_TRANSFER_H
#include "dolly_amiga_surface_scan.h"
#include "dolly_ffs_analyze.h"
#include "dolly_amiga_restore_source.h"
#include "dolly_amiga_write.h"
#include "dolly_restore_transfer.h"
#include "dolly_dlrp.h"
struct MsgPort;

typedef struct DollyAmigaTransfer {
    DollySurfaceScan reader;
    DollyAmigaRestoreSource image;
    DollyRestoreTransfer job;
    DollyAmigaWriteTarget target;
    DollyPhysicalEntry device;
    DollyMediaGeometry geometry;
    DollyIoHints io;
    char device_name[112];
    struct MsgPort *port;
    struct IOStdReq *request;
    DollyBlockSource *copy_extent,*resized_extent;
    DollyFfsReport resize_plan;
    void *buffer;
    void *resume_scratch;
    DollyTransferRange *smart_ranges;
    size_t smart_range_count,smart_range_capacity;
    uint32_t source_first_crc,source_last_crc,target_first_crc,target_last_crc;
    uint32_t target_partition_dos_type;
    DollyMountList source_mounts,target_mounts,observed_mounts;
    struct Window *parent;
    unsigned minimum_opens,expected_opens;
    int check_source_mounts,check_target_mounts,may_have_written;
    int opened,restoring,resizing,destination_reload_needed,mount_destination;
    unsigned mounted_count,mount_skipped;
    const char *failure;
    char validation_failure[112];
} DollyAmigaTransfer;

void dolly_amiga_analyze_resize(struct Window *parent,const DollyEndpoint *source,
                              const DollyEndpoint *destination,
                              DollyFfsProgress progress);
int dolly_amiga_transfer_open(DollyAmigaTransfer *transfer,struct Window *parent,
                     const DollyEndpoint *source,const DollyEndpoint *destination,int restore,unsigned copy_mode,
                     DollyFfsProgress smart_progress,void (*show_status)(const char *));
int dolly_amiga_transfer_resume(DollyAmigaTransfer *transfer,struct Window *parent,
                     const DollyDlrpTransferResume *resume,void (*show_status)(const char *));
/* Offer optional name changes after a completed copy or restore, while
 * the destination is still held and its filesystems remain offline. */
void dolly_amiga_transfer_review_names(DollyAmigaTransfer *transfer,
                                      const DollyEndpoint *destination,
                                      const char *summary,
                                      void (*show_status)(const char *));
void dolly_amiga_transfer_step(DollyAmigaTransfer *transfer);
void dolly_amiga_transfer_close(DollyAmigaTransfer *transfer);
int dolly_amiga_prepare_rdb_unmount(struct Window *parent,const char *driver,ULONG unit,
                uint64_t capacity,BPTR image_lock,BPTR recovery_drawer);
#endif
