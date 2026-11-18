#include "dolly_amiga_surface_scan.h"
#include "dolly_amiga_device_query.h"
#include "dolly_amiga_partition_scan.h"
#include "dolly_amiga_physical_catalog.h"
#include "dolly_amiga_source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void report_dma(DollyDmaStatus status,const char *message,void *user) {
    DollySurfaceScan *scan=user;
    (void)status;
    scan->failure=message;
}

void dolly_surface_close(DollySurfaceScan *scan) {
    if(scan->source)dolly_block_close(scan->source);
    free(scan->buffer);
    scan->source=NULL;
    scan->buffer=NULL;
}

int dolly_surface_open(DollySurfaceScan *scan,const DollyEndpoint *endpoint) {
    DollyPhysicalEntry device;
    DollyMediaGeometry geometry;
    DollyAmigaDeviceOptions opts;
    DollyBlockStatus status;
    const char *tail=NULL;
    char name[112],size[40];
    ULONG logical_sector=0;
    uint64_t capacity,start=0,length=0;
    uint32_t transfer;
    dolly_surface_close(scan);
    dolly_surface_log_clear(&scan->log);
    memset(scan,0,sizeof(*scan));
    scan->failure="Source has no validated device range.";
    if(endpoint->kind==DOLLY_ENDPOINT_LOGICAL) {
        if(!dolly_logical_extent(endpoint->path,&device,&start,&length,&logical_sector))
            return 0;
    } else {
        if(endpoint->kind!=DOLLY_ENDPOINT_UNIT && endpoint->kind!=DOLLY_ENDPOINT_PARTITION)
            return 0;
        tail=dolly_amiga_parse_unit(endpoint->path,device.driver,sizeof(device.driver),&device.unit);
        if(!tail)return 0;
    }
    scan->failure="Scan requires a verified controller profile.";
    /* UAE-IDE reports a distinct INQUIRY vendor. The driver name alone
     * also identifies real controllers and must not authorize a scan. */
    if(strcmp(device.driver,"scsi.device")!=0)return 0;
    if(!dolly_amiga_device_query(device.driver,device.unit,name,sizeof(name),
                                size,sizeof(size),&geometry,1) ||
       !geometry.capacity_valid) {
        scan->failure="Cannot verify source geometry.";
        return 0;
    }
    if(strncmp(name,"UAE-IDE ",8)!=0 && strcmp(name,"UAE-IDE")!=0)return 0;
    if(geometry.block_bytes<512u || geometry.block_bytes>32768u) {
        scan->failure="Unsupported scan sector size.";
        return 0;
    }
    capacity=geometry.block_count*geometry.block_bytes;
    if(endpoint->kind==DOLLY_ENDPOINT_UNIT)length=capacity;
    if(endpoint->kind==DOLLY_ENDPOINT_PARTITION) {
        DollyNativePartitionMap *map=malloc(sizeof(*map));
        int ok=0;
        if(!map) {
            scan->failure="Not enough memory for partition scan.";
            return 0;
        }
        if(*tail && dolly_amiga_partition_scan(device.driver,device.unit,&geometry,map) &&
           endpoint->partition<map->count) {
            const DollyNativePartition *part=&map->parts[endpoint->partition];
            start=part->start_bytes;
            length=part->length_bytes;
            ok=strcmp(tail+3,part->label)==0 &&
               start==endpoint->partition_start_bytes &&
               length==endpoint->partition_length_bytes;
        }
        free(map);
        if(!ok) {
            scan->failure="Partition changed. Query and select it again.";
            return 0;
        }
    }
    scan->failure="Invalid or unsupported scan extent.";
    if(!length || (logical_sector && logical_sector!=geometry.block_bytes) ||
       start>capacity || length>capacity-start ||
       start%geometry.block_bytes || length%geometry.block_bytes)return 0;
    scan->sector_bytes=geometry.block_bytes;
    scan->first_lba=start/scan->sector_bytes;
    scan->total=length/scan->sector_bytes;
    transfer=scan->sector_bytes>DOLLY_MEDIA_TRANSFER_BYTES?scan->sector_bytes:DOLLY_MEDIA_TRANSFER_BYTES;
    scan->buffer=malloc(transfer);
    if(!scan->buffer) {
        scan->failure="Not enough memory for scan buffer.";
        return 0;
    }
    memset(&opts,0,sizeof(opts));
    opts.device_name=device.driver;
    opts.unit=device.unit;
    opts.capacity_bytes=capacity;
    opts.physical_block_bytes=scan->sector_bytes;
    opts.max_transfer_bytes=transfer;
    opts.dma_address_mask=0x00fffffeu;
    opts.dma_alignment=2;
    opts.dma_profile_verified=1;
    opts.require_24bit_dma=1;
    opts.report_dma_problem=report_dma;
    opts.report_user=scan;
    opts.required_read_end=start+length;
    opts.verified_uae_ide=1;
    scan->failure=NULL;
    status=dolly_amiga_open_device(&opts,&scan->source);
    if(status!=DOLLY_BLOCK_OK) {
        if(!scan->failure)
            scan->failure=status==DOLLY_BLOCK_EUNSUPPORTED?
                          "Driver has no supported large-offset read.":
                          dolly_block_status_name(status);
        dolly_surface_close(scan);
        return 0;
    }
    strcpy(scan->source_path,endpoint->path);
    strcpy(scan->device_name,name);
    scan->geometry=geometry;
    scan->device=device;
    printf("SURFACE SCAN: %s unit=%lu first LBA=%lu sectors=%lu\n",
           device.driver,(unsigned long)device.unit,(unsigned long)scan->first_lba,
           (unsigned long)scan->total);
    return 1;
}

void dolly_surface_step(DollySurfaceScan *scan) {
    DollyBlockReadResult read;
    uint32_t batch,count;
    uint64_t lba;
    if(!scan->source || scan->failure || scan->processed>=scan->total)return;
    batch=DOLLY_MEDIA_TRANSFER_BYTES/scan->sector_bytes;
    lba=scan->first_lba+scan->processed;
    if(!batch)batch=1;
    count=scan->fallback?1u:batch;
    if(count>scan->total-scan->processed)count=(uint32_t)(scan->total-scan->processed);
    read=dolly_block_read(scan->source,lba*scan->sector_bytes,
                         scan->buffer,(size_t)count*scan->sector_bytes);
    if(read.status!=DOLLY_BLOCK_OK) {
        if(read.status!=DOLLY_BLOCK_EIO && read.status!=DOLLY_BLOCK_ESHORT) {
            dolly_surface_log_add(&scan->log,DOLLY_SURFACE_ABORTED,
                                  lba,count,read.status);
            scan->failure=dolly_block_status_name(read.status);
            return;
        }
        if(count>1u) {
            dolly_surface_log_add(&scan->log,DOLLY_SURFACE_BATCH_FAILED,
                                  lba,count,read.status);
            scan->fallback=count;
            return;
        }
        if(!scan->retry_pending) {
            dolly_surface_log_add(&scan->log,DOLLY_SURFACE_RETRY,lba,1,read.status);
            scan->retry_pending=1;
            ++scan->retries;
            return;
        }
        dolly_surface_log_add(&scan->log,DOLLY_SURFACE_UNREADABLE,lba,1,read.status);
        ++scan->errors;
        {
            DollySurfaceEvent event={lba,1,DOLLY_SURFACE_UNREADABLE,read.status};
            char line[DOLLY_LOG_LINE_SIZE];
            dolly_surface_log_line(line,&event,&scan->geometry);
            printf("SURFACE SCAN: %s\n",line);
        }
    } else if(scan->retry_pending) {
        dolly_surface_log_add(&scan->log,DOLLY_SURFACE_RECOVERED,lba,1,read.status);
    }
    scan->retry_pending=0;
    if(scan->fallback)--scan->fallback;
    scan->processed+=count;
}
