#ifndef DOLLY_AMIGA_TRANSFER_H
#define DOLLY_AMIGA_TRANSFER_H
#include "dolly_amiga_surface_scan.h"
#include "dolly_amiga_restore_source.h"
#include "dolly_amiga_write.h"
#include "dolly_restore_transfer.h"
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
    DollyBlockSource *copy_extent;
    void *buffer;
    uint32_t source_first_crc,source_last_crc,target_first_crc,target_last_crc;
    DollyMountList source_mounts,target_mounts,observed_mounts;
    struct Window *parent;
    unsigned expected_opens;
    int check_source_mounts,check_target_mounts;
    int opened,restoring;
    const char *failure;
} DollyAmigaTransfer;

int dolly_amiga_transfer_open(DollyAmigaTransfer *transfer,struct Window *parent,
                     const DollyEndpoint *source,const DollyEndpoint *destination,int restore);
void dolly_amiga_transfer_step(DollyAmigaTransfer *transfer);
void dolly_amiga_transfer_close(DollyAmigaTransfer *transfer);
#endif
