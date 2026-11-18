#ifndef DOLLY_AMIGA_SURFACE_SCAN_H
#define DOLLY_AMIGA_SURFACE_SCAN_H
#include "dolly_block_source.h"
#include "dolly_endpoint.h"
#include "dolly_surface_log.h"
#include "dolly_amiga_physical_catalog.h"
#define DOLLY_MEDIA_TRANSFER_BYTES 16384u

typedef struct DollySurfaceScan {
    DollyBlockSource *source;
    void *buffer;
    uint64_t first_lba,total,processed;
    uint32_t sector_bytes,transfer_bytes,fallback,errors,retries;
    unsigned retry_pending;
    const char *failure;
    DollySurfaceLog log;
    DollyMediaGeometry geometry;
    DollyPhysicalEntry device;
    char device_name[112];
    char source_path[sizeof(((DollyEndpoint *)0)->path)];
} DollySurfaceScan;

/* Initialize scan to zero before its first open. Each step issues at most
 * one bounded request. Caller drives steps from its event loop. */
struct Window;
int dolly_surface_open(DollySurfaceScan *scan,const DollyEndpoint *endpoint,
                       struct Window *parent);
void dolly_surface_step(DollySurfaceScan *scan);
/* Close releases device resources and retains the completed log.
 * Clear scan->log when discarding the scan or shutting down. */
void dolly_surface_close(DollySurfaceScan *scan);
#endif
