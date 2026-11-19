#include "dolly_amiga_surface_scan.h"
#include "dolly_amiga_io_settings.h"
#include "dolly_amiga_destructive.h"
#include <intuition/intuition.h>
#include <proto/intuition.h>
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

int dolly_surface_open(DollySurfaceScan *scan,const DollyEndpoint *endpoint,
                       struct Window *parent,DollyIoRole role,int configure,DollyWarnings *warnings) {
    DollyPhysicalEntry device;
    DollyMediaGeometry geometry;
    DollyAmigaDeviceOptions opts;
    DollyIoHints hints={0},manual;
    DollyBlockStatus status;
    const char *tail=NULL;
    char name[112],size[40];
    ULONG logical_sector=0;
    uint64_t capacity,start=0,length=0;
    DollyOutsideRange policy=DOLLY_RANGE_STRICT;
    uint32_t transfer,batch_limit;
    int uae_ide,complete_rdb=0;
    dolly_surface_close(scan);
    dolly_surface_log_clear(&scan->log);
    memset(scan,0,sizeof(*scan));
    scan->failure="No validated device range.";
    if(endpoint->kind==DOLLY_ENDPOINT_LOGICAL) {
        if(!dolly_logical_extent(endpoint->path,&device,&start,&length,&logical_sector,NULL))
            return 0;
    } else {
        if(endpoint->kind!=DOLLY_ENDPOINT_UNIT && endpoint->kind!=DOLLY_ENDPOINT_PARTITION)
            return 0;
        tail=dolly_amiga_parse_unit(endpoint->path,device.driver,sizeof(device.driver),&device.unit);
        if(!tail)return 0;
    }
    manual=dolly_amiga_io_override(&device);
    if(!dolly_amiga_device_query(device.driver,device.unit,name,sizeof(name),
                                size,sizeof(size),&geometry,1,
                                scan->ata_identify,sizeof(scan->ata_identify)) ||
       !geometry.capacity_valid) {
        scan->failure="Cannot verify device geometry.";
        return 0;
    }
    uae_ide=strcmp(device.driver,"scsi.device")==0 &&
            (strncmp(name,"UAE-IDE ",8)==0 || strcmp(name,"UAE-IDE")==0);
    if(geometry.block_bytes<512u || geometry.block_bytes>32768u) {
        scan->failure="Unsupported scan sector size.";
        return 0;
    }
    capacity=geometry.block_count*geometry.block_bytes;
    if(endpoint->kind==DOLLY_ENDPOINT_UNIT)length=capacity;
    if(geometry.block_bytes==512u) {
        DollyNativePartitionMap *map=malloc(sizeof(*map));
        int ok,found;
        unsigned i;
        if(!map) {
            scan->failure="Not enough memory for partition scan.";
            return 0;
        }
        found=dolly_amiga_partition_scan(device.driver,device.unit,&geometry,map);
        scan->has_rdb=found && map->format==DOLLY_NATIVE_PARTITION_RDB;
        ok=endpoint->kind!=DOLLY_ENDPOINT_PARTITION;
        if(found && endpoint->kind==DOLLY_ENDPOINT_UNIT) {
            complete_rdb=map->format==DOLLY_NATIVE_PARTITION_RDB && map->count!=0 &&
                         !map->rdb_capacity_mismatch;
            for(i=0;i<map->count;++i) {
                if(map->parts[i].io_hints.present!=DOLLY_IO_ALL)complete_rdb=0;
                dolly_io_hints_merge(&hints,map->parts[i].io_hints);
            }
        } else if(found && endpoint->kind==DOLLY_ENDPOINT_LOGICAL &&
                  map->format==DOLLY_NATIVE_PARTITION_RDB) {
            DollyIoHints disk_hints={0};
            for(i=0;i<map->count;++i) {
                const DollyNativePartition *part=&map->parts[i];
                if(!part->outside_media && part->start_bytes==start && part->length_bytes==length)
                    dolly_io_hints_merge(&disk_hints,part->io_hints);
            }
            hints=disk_hints;
            complete_rdb=hints.present==DOLLY_IO_ALL;
        } else if(found && endpoint->kind==DOLLY_ENDPOINT_PARTITION &&
                  *tail && endpoint->partition<map->count) {
            const DollyNativePartition *part=&map->parts[endpoint->partition];
            hints=part->io_hints;
            complete_rdb=part->is_rdb && hints.present==DOLLY_IO_ALL;
            start=part->start_bytes;
            length=part->length_bytes;
            ok=strcmp(tail+3,part->label)==0 &&
               start==endpoint->partition_start_bytes &&
               length==endpoint->partition_length_bytes;
            if(ok && part->outside_media) {
                policy=endpoint->outside_range;
                ok=dolly_amiga_allow_outside_range() &&
                   endpoint->confirmed_capacity_bytes==capacity &&
                   (policy==DOLLY_RANGE_ZERO_FILL || policy==DOLLY_RANGE_AS_IS);
            }
        }
        free(map);
        if(!ok) {
            scan->failure="Partition changed. Query and select it again.";
            return 0;
        }
    }
    scan->failure="Invalid or unsupported scan extent.";
    if(!length || (logical_sector && logical_sector!=geometry.block_bytes) ||
       start>UINT64_MAX-length ||
       (policy==DOLLY_RANGE_STRICT && (start>capacity || length>capacity-start)) ||
       start%geometry.block_bytes || length%geometry.block_bytes)return 0;
    scan->sector_bytes=geometry.block_bytes;
    scan->first_lba=start/scan->sector_bytes;
    scan->total=length/scan->sector_bytes;
    scan->max_retries=1;
    if(configure && !dolly_amiga_scan_options(scan,parent,&geometry)) {
        scan->failure="Scan setup cancelled.";
        return 0;
    }
    start=scan->first_lba*scan->sector_bytes;
    length=scan->total*scan->sector_bytes;
    if(!complete_rdb) {
        char reason[310];
        snprintf(reason,sizeof(reason),
                "%s: %s / Unit %lu\n"
                "RDB I/O settings are missing or incomplete (Mask, MaxTransfer, BufMemType).\n"
                "Dolly will use conservative defaults for missing fields and apply your manual overrides.",
                role==DOLLY_IO_DESTINATION?"Destination":"Source",
                device.driver,(unsigned long)device.unit);
        DollyIoHints fallback={0x00fffffeu,DOLLY_IO_MEM_CHIP|DOLLY_IO_MEM_24BIT,
                               geometry.block_bytes>DOLLY_MEDIA_TRANSFER_BYTES?
                               geometry.block_bytes:DOLLY_MEDIA_TRANSFER_BYTES,DOLLY_IO_ALL};
        DollyWarning *warning=dolly_warning_add(warnings,
            role==DOLLY_IO_DESTINATION?"Destination I/O":"Source I/O",reason,"Continue");
        if(!warning) {
            scan->failure="Cannot collect I/O warning.";
            return 0;
        }
        warning->settings=endpoint;
        dolly_io_hints_override(&fallback,hints);
        hints=fallback;
    }
    dolly_io_hints_override(&hints,manual);
    if(hints.buffer_memory&DOLLY_IO_MEM_24BIT) {
        hints.mask&=0x00ffffffu;
    }
    scan->failure="Invalid I/O parameters. Review the I/O override.";
    if(!dolly_io_hints_valid(hints))return 0;
    batch_limit=scan->sector_bytes>DOLLY_MEDIA_TRANSFER_BYTES?
                scan->sector_bytes:DOLLY_MEDIA_TRANSFER_BYTES;
    transfer=hints.max_transfer<batch_limit?hints.max_transfer:batch_limit;
    transfer-=transfer%scan->sector_bytes;
    if(!transfer)return 0;
    scan->transfer_bytes=transfer;
    scan->buffer=malloc(transfer);
    if(!scan->buffer) {
        scan->failure="Not enough memory for scan buffer.";
        return 0;
    }
    memset(&opts,0,sizeof(opts));
    opts.device_name=device.driver;
    opts.unit=device.unit;
    scan->outside_range=policy;
    opts.capacity_bytes=policy==DOLLY_RANGE_AS_IS && start+length>capacity?
                        start+length:capacity;
    opts.physical_block_bytes=scan->sector_bytes;
    opts.max_transfer_bytes=transfer;
    opts.dma_address_mask=hints.mask;
    opts.dma_alignment=dolly_io_alignment(hints.mask);
    opts.buffer_memory=hints.buffer_memory;
    opts.dma_profile_verified=1;
    opts.require_24bit_dma=(uint8_t)((hints.buffer_memory&DOLLY_IO_MEM_24BIT)!=0);
    opts.report_dma_problem=report_dma;
    opts.report_user=scan;
    opts.required_read_end=policy==DOLLY_RANGE_ZERO_FILL && start+length>capacity?
                           capacity:start+length;
    opts.verified_uae_ide=(uint8_t)uae_ide;
    opts.bypass_legacy_scsi_limit=(uint8_t)dolly_amiga_legacy_override();
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
    printf("I/O: Mask=%08lx BufMemType=%08lx MaxTransfer=%lu Batch=%lu\n",
           (unsigned long)hints.mask,(unsigned long)hints.buffer_memory,
           (unsigned long)hints.max_transfer,(unsigned long)transfer);
    strcpy(scan->source_path,endpoint->path);
    strcpy(scan->device_name,name);
    scan->geometry=geometry;
    scan->effective_io=hints;
    scan->device=device;
    printf("SURFACE SCAN: %s unit=%lu first LBA=%lu sectors=%lu\n",
           device.driver,(unsigned long)device.unit,(unsigned long)scan->first_lba,
           (unsigned long)scan->total);
    return 1;
}

DollyBlockReadResult dolly_surface_read(DollySurfaceScan *scan,uint64_t offset,
                                       void *buffer,size_t bytes) {
    uint64_t capacity=scan->geometry.block_count*scan->sector_bytes;
    size_t readable=bytes;
    DollyBlockReadResult result={DOLLY_BLOCK_OK,0};
    if(scan->outside_range!=DOLLY_RANGE_ZERO_FILL)
        return dolly_block_read(scan->source,offset,buffer,bytes);
    if(offset>=capacity)readable=0;
    else if(bytes>capacity-offset)readable=(size_t)(capacity-offset);
    if(readable) {
        result=dolly_block_read(scan->source,offset,buffer,readable);
        if(result.status!=DOLLY_BLOCK_OK || result.bytes_read!=readable)return result;
    }
    memset((unsigned char *)buffer+readable,0,bytes-readable);
    return (DollyBlockReadResult){DOLLY_BLOCK_OK,bytes};
}

void dolly_surface_step(DollySurfaceScan *scan) {
    DollyBlockReadResult read;
    uint32_t batch,count;
    uint64_t lba;
    if(!scan->source || scan->failure || scan->processed>=scan->total)return;
    batch=scan->transfer_bytes/scan->sector_bytes;
    lba=scan->first_lba+scan->processed;
    if(!batch)batch=1;
    count=scan->fallback?1u:batch;
    if(count>scan->total-scan->processed)count=(uint32_t)(scan->total-scan->processed);
    if(scan->outside_range==DOLLY_RANGE_ZERO_FILL) {
        uint64_t driver_end=scan->geometry.block_count;
        if(lba>=driver_end) {
            dolly_surface_log_add(&scan->log,DOLLY_SURFACE_UNREADABLE,
                                  lba,count,DOLLY_BLOCK_ERANGE);
            scan->errors+=count;
            scan->processed+=count;
            return;
        }
        if(count>driver_end-lba)count=(uint32_t)(driver_end-lba);
    }
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
        if(scan->retry_pending<scan->max_retries) {
            dolly_surface_log_add(&scan->log,DOLLY_SURFACE_RETRY,lba,1,read.status);
            ++scan->retry_pending;
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
