#ifndef DOLLY_AMIGA_SURFACE_SCAN_H
#define DOLLY_AMIGA_SURFACE_SCAN_H
#include "dolly_block_source.h"
#include "dolly_endpoint.h"
#include "dolly_surface_log.h"
#include "dolly_amiga_physical_catalog.h"
#include "dolly_amiga_warnings.h"
#define DOLLY_MEDIA_TRANSFER_BYTES 16384u

typedef struct DollySurfaceScan {
    DollyBlockSource *source;
    void *buffer;
    uint64_t first_lba,total,processed;
    uint32_t sector_bytes,transfer_bytes,fallback,errors,retries;
    uint32_t rdb_size_block,rdb_sectors_per_block,rdb_reserved_blocks,rdb_dos_type;
    unsigned is_rdb_partition;
    unsigned retry_pending,max_retries;
    struct DollyReadGuard *mount_guard;
    const char *failure;
    DollySurfaceLog log;
    DollyMediaGeometry geometry;
    DollyOutsideRange outside_range;
    char ata_identify[88];
    DollyIoHints effective_io;
    DollyPhysicalEntry device;
    char device_name[112];
    char source_path[sizeof(((DollyEndpoint *)0)->path)];
} DollySurfaceScan;

/* Initialize scan to zero before its first open. Each step issues at most
 * one bounded request. Caller drives steps from its event loop. */
struct Window;
typedef enum DollyIoRole {
    DOLLY_IO_SOURCE,
    DOLLY_IO_DESTINATION
} DollyIoRole;
int dolly_surface_open(DollySurfaceScan *scan,const DollyEndpoint *endpoint,
                       struct Window *parent,DollyIoRole role,int configure,DollyWarnings *warnings);
int dolly_amiga_scan_options(DollySurfaceScan *scan,struct Window *parent,
                            const DollyMediaGeometry *geometry);
DollyBlockReadResult dolly_surface_read(DollySurfaceScan *scan,uint64_t offset,
                                       void *buffer,size_t bytes);
void dolly_surface_step(DollySurfaceScan *scan);
/* Close releases device resources and retains the completed log.
 * Clear scan->log when discarding the scan or shutting down. */
void dolly_surface_close(DollySurfaceScan *scan);
#endif
