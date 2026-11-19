#include "dolly_amiga_transfer.h"
#include "dolly_restore_check.h"
#include "dolly_checksum.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_transfer_guard.h"
#include "dolly_amiga_source.h"
#include "dolly_amiga_io_settings.h"
#include "dolly_amiga_partition_scan.h"
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int raw_endpoint(const DollyEndpoint *endpoint) {
    return endpoint->kind==DOLLY_ENDPOINT_UNIT || endpoint->kind==DOLLY_ENDPOINT_PARTITION;
}
static uint64_t start_of(const DollySurfaceScan *s) {
    return s->first_lba*s->sector_bytes;
}
static uint64_t length_of(const DollySurfaceScan *s) {
    return s->total*s->sector_bytes;
}
static int rdb_or_empty(const DollySurfaceScan *scan,const DollyEndpoint *endpoint) {
    DollyNativePartitionMap *map=malloc(sizeof(*map));
    int ok=0;
    if(!map)return 0;
    if(dolly_amiga_partition_scan(scan->device.driver,scan->device.unit,&scan->geometry,map)) {
        if(endpoint->kind==DOLLY_ENDPOINT_UNIT)
            ok=(map->format==DOLLY_NATIVE_PARTITION_NONE ||
                (map->format==DOLLY_NATIVE_PARTITION_RDB && !map->rdb_capacity_mismatch));
        else if(endpoint->partition<map->count) {
            const DollyNativePartition *part=&map->parts[endpoint->partition];
            char driver[DOLLY_PHYS_DRIVER_LEN];
            ULONG unit;
            const char *tail=dolly_amiga_parse_unit(endpoint->path,driver,sizeof(driver),&unit);
            ok=tail && *tail && !strcmp(tail+3,part->label) &&
               part->is_rdb && !part->outside_media &&
               part->start_bytes==start_of(scan) && part->length_bytes==length_of(scan);
        }
    }
    free(map);
    return ok;
}
static DollyBlockReadResult read_extent(void *context,uint64_t offset,
                                       void *data,size_t bytes) {
    DollySurfaceScan *source=context;
    return dolly_surface_read(source,start_of(source)+offset,data,bytes);
}
static void keep_reader(void *context) {
    (void)context; /* DollyAmigaTransfer owns the underlying reader. */
}
static int target_idle(const DollyAmigaTransfer *t) {
    return t->request->io_Device &&
           t->request->io_Device->dd_Library.lib_OpenCnt==t->own_opens &&
           dolly_amiga_unit_registered(t->device.driver,t->device.unit)==0;
}
static DollyBlockWriteStatus write_extent(void *context,uint64_t offset,
                                      const void *data,size_t bytes,int verify) {
    DollyAmigaTransfer *t=context;
    if(!target_idle(t)) {
        t->failure="Destination became busy. Write stopped.";
        return DOLLY_WRITE_REJECTED;
    }
    return dolly_amiga_write_batch(&t->target,offset,data,bytes,verify);
}
void dolly_amiga_transfer_close(DollyAmigaTransfer *t) {
    if(t->opened)CloseDevice((struct IORequest *)t->request);
    if(t->request)DeleteIORequest((struct IORequest *)t->request);
    if(t->port)DeleteMsgPort(t->port);
    if(t->target.bounce)FreeMem(t->target.bounce,t->target.bounce_bytes);
    free(t->buffer);
    dolly_block_close(t->copy_extent);
    dolly_amiga_restore_source_close(&t->image);
    dolly_surface_close(&t->reader);
    dolly_surface_log_clear(&t->reader.log);
    t->request=NULL;
    t->port=NULL;
    t->target.bounce=NULL;
    t->buffer=NULL;
    t->copy_extent=NULL;
    t->opened=0;
}

static DollyRestoreAssessment assess_image(const DollyAmigaTransfer *t,
                      DollyBlockSource *target,const DollyEndpoint *destination) {
    uint32_t sector=t->target.sector_bytes;
    DollyRestoreExtent image={t->image.blocks,0,dolly_block_size(t->image.blocks),
                             sector,DOLLY_RESTORE_UNKNOWN};
    DollyRestoreExtent disk={target,t->target.start,t->target.length,sector,
        destination->kind==DOLLY_ENDPOINT_UNIT?DOLLY_RESTORE_RDB_DISK:DOLLY_RESTORE_PARTITION};
    return dolly_restore_assess(&image,&disk);
}

/* Read using the same handle and bounce buffer that will submit writes. */
static DollyBlockReadResult read_target(void *context,uint64_t offset,
                                       void *buffer,size_t bytes) {
    DollyAmigaTransfer *t=context;
    DollyBlockReadResult result={DOLLY_BLOCK_EIO,0};
    if(!bytes || bytes>t->target.bounce_bytes || offset>=UINT64_C(0x100000000) ||
       bytes>UINT64_C(0x100000000)-offset || offset%t->target.sector_bytes ||
       bytes%t->target.sector_bytes)return result;
    t->request->io_Command=CMD_READ;
    t->request->io_Flags=0;
    t->request->io_Error=0;
    t->request->io_Offset=(ULONG)offset;
    t->request->io_Actual=0;
    t->request->io_Data=t->target.bounce;
    t->request->io_Length=(ULONG)bytes;
    if(DoIO((struct IORequest *)t->request) || t->request->io_Error)return result;
    result.bytes_read=t->request->io_Actual;
    if(result.bytes_read!=bytes) {
        result.bytes_read=0;
        result.status=DOLLY_BLOCK_ESHORT;
        return result;
    }
    memcpy(buffer,t->target.bounce,bytes);
    result.status=DOLLY_BLOCK_OK;
    return result;
}

static int extent_anchors(DollyBlockSource *blocks,uint64_t start,uint64_t length,
                          uint32_t sector,void *buffer,uint32_t *first,uint32_t *last) {
    DollyChecksum checksum;
    if(dolly_block_read(blocks,start,buffer,sector).status!=DOLLY_BLOCK_OK)return 0;
    dolly_checksum_init(&checksum);
    dolly_checksum_update(&checksum,buffer,sector);
    *first=dolly_checksum_final(&checksum);
    if(dolly_block_read(blocks,start+length-sector,buffer,sector).status!=DOLLY_BLOCK_OK)return 0;
    dolly_checksum_init(&checksum);
    dolly_checksum_update(&checksum,buffer,sector);
    *last=dolly_checksum_final(&checksum);
    return 1;
}

int dolly_amiga_transfer_open(DollyAmigaTransfer *t,struct Window *parent,
                     const DollyEndpoint *source,const DollyEndpoint *destination,int restore) {
    DollySurfaceScan disk={0};
    DollyBlockSource *input=NULL,*locked_reader=NULL;
    DollyRestoreAssessment approval={0};
    DollyMediaGeometry live={0};
    char live_name[112],size[40],reason[620],a[21],b[21];
    const char *problem=NULL;
    ULONG batch,verify=dolly_amiga_verify_writes();
    uint32_t first,last;
    int known_driver=0;
    dolly_amiga_transfer_close(t);
    memset(t,0,sizeof(*t));
    t->restoring=restore;
    if(!raw_endpoint(destination) || destination->outside_range!=DOLLY_RANGE_STRICT ||
       (!restore && (!raw_endpoint(source) || source->outside_range!=DOLLY_RANGE_STRICT))) {
        problem="Choose a whole disk or an in-range RDB partition. Restore requires a RAW/HDF file.";
        goto fail;
    }
    if(!restore && !dolly_surface_open(&t->reader,source,parent,DOLLY_IO_SOURCE,0)) {
        problem=t->reader.failure;
        goto fail;
    }
    if(!dolly_surface_open(&disk,destination,parent,DOLLY_IO_DESTINATION,0)) {
        problem=disk.failure;
        goto fail;
    }
    t->device=disk.device;
    t->geometry=disk.geometry;
    t->io=disk.effective_io;
    strcpy(t->device_name,disk.device_name);
    t->target.start=start_of(&disk);
    t->target.length=length_of(&disk);
    t->target.capacity=disk.geometry.block_count*disk.sector_bytes;
    t->target.sector_bytes=disk.sector_bytes;
    if(t->target.start>=UINT64_C(0x100000000) ||
       t->target.length>UINT64_C(0x100000000)-t->target.start) {
        problem="Destination extends beyond the legacy writer's 4 GiB address range.";
        goto fail;
    }
    if(!rdb_or_empty(&disk,destination) || (!restore && !rdb_or_empty(&t->reader,source))) {
        problem="Only validated RDB or unpartitioned 512-byte-sector media are supported.";
        goto fail;
    }
    if(!strcasecmp(t->device.driver,"scsi.device"))
        known_driver=!strncmp(t->device_name,"UAE-IDE ",8) ||
                     !strcmp(t->device_name,"UAE-IDE") || dolly_amiga_probe_rom_scsi(t->device.unit);
    if(dolly_amiga_unit_registered(t->device.driver,t->device.unit)!=0) {
        problem="Destination is registered with AmigaDOS or its offline status cannot be verified.";
        goto fail;
    }
    batch=disk.transfer_bytes;
    if(restore) {
        DollyBlockStatus status=dolly_amiga_restore_source_open(&t->image,source,disk.sector_bytes);
        if(status!=DOLLY_BLOCK_OK) {
            snprintf(reason,sizeof(reason),"Cannot open a flat RAW/HDF image: %s.",
                     dolly_block_status_name(status));
            problem=reason;
            goto fail;
        }
        approval=assess_image(t,disk.source,destination);
        if(approval.decision==DOLLY_RESTORE_BLOCKED ||
           (approval.decision!=DOLLY_RESTORE_MATCH && !dolly_amiga_allow_restore_mismatch())) {
            problem=approval.reason;
            goto fail;
        }
        if(dolly_amiga_guard_file_transfer(parent,t->image.lock,&disk.device,
                                           t->target.start,t->target.length))goto cancel;
        input=t->image.blocks;
    } else {
        if(length_of(&t->reader)!=t->target.length || t->reader.sector_bytes!=disk.sector_bytes) {
            dolly_surface_log_decimal(a,length_of(&t->reader));
            dolly_surface_log_decimal(b,t->target.length);
            snprintf(reason,sizeof(reason),"Source: %s bytes, %lu B/sector.\n"
                     "Destination: %s bytes, %lu B/sector.\nCopy requires equal sizes and sector widths.",
                     a,(unsigned long)t->reader.sector_bytes,b,(unsigned long)disk.sector_bytes);
            problem=reason;
            goto fail;
        }
        if(!strcasecmp(t->reader.device.driver,disk.device.driver) &&
           t->reader.device.unit==disk.device.unit) {
            if(dolly_unit_relation(start_of(&t->reader),length_of(&t->reader),
                    t->target.start,t->target.length)!=DOLLY_UNIT_RANGE_DISJOINT) {
                problem="Source and destination overlap on the same unit.";
                goto fail;
            }
            dolly_amiga_warn_same_unit(parent);
        }
        if(batch>t->reader.transfer_bytes)batch=t->reader.transfer_bytes;
        if(dolly_block_open_backend(length_of(&t->reader),read_extent,keep_reader,
                                    &t->reader,&t->copy_extent)!=DOLLY_BLOCK_OK) {
            problem="Cannot allocate the copy source.";
            goto fail;
        }
        input=t->copy_extent;
    }

    if(!extent_anchors(input,0,t->target.length,disk.sector_bytes,disk.buffer,
                       &t->source_first_crc,&t->source_last_crc) ||
       !extent_anchors(disk.source,t->target.start,t->target.length,disk.sector_bytes,disk.buffer,
                       &t->target_first_crc,&t->target_last_crc)) {
        problem="Could not read the first and last sectors of both extents.";
        goto fail;
    }
    dolly_surface_log_decimal(a,t->target.start/disk.sector_bytes);
    dolly_surface_log_decimal(b,disk.total);
    snprintf(reason,sizeof(reason),"%s\nSource: %.160s\nDestination: %.160s\n"
             "Write %s sectors starting at LBA %s. Verification: %s.%s%.155s",
             restore?"Restore image":"Sector copy",source->path,destination->path,b,a,
             verify?"on":"off",approval.reason[0]?"\nProceed at your own risk.\n":"",
             approval.reason);
    if(!dolly_amiga_confirm_destructive(parent,reason))goto cancel;

    /* Recheck the selected geometry and partition after the countdown.
     * Keep only our copy reader and the single target Exec handle open. */
    dolly_surface_close(&disk);
    if(!dolly_amiga_device_query(t->device.driver,t->device.unit,live_name,sizeof(live_name),
                                 size,sizeof(size),&live,1,NULL,0) ||
       !live.capacity_valid || strcmp(live_name,t->device_name) ||
       live.block_count!=t->geometry.block_count || live.block_bytes!=t->geometry.block_bytes ||
       live.cylinders!=t->geometry.cylinders || live.heads!=t->geometry.heads ||
       live.sectors_per_track!=t->geometry.sectors_per_track ||
       !rdb_or_empty(&disk,destination) || (!restore && !rdb_or_empty(&t->reader,source))) {
        problem="Media geometry or partition selection changed. Query and select it again.";
        goto fail;
    }
    t->port=CreateMsgPort();
    if(t->port)t->request=CreateIORequest(t->port,sizeof(*t->request)+128u);
    t->target.bounce_bytes=batch;
    t->target.bounce=AllocMem(batch,MEMF_PUBLIC|t->io.buffer_memory);
    t->buffer=malloc(batch);
    t->target.dma_mask=t->io.mask;
    t->target.dma_alignment=dolly_io_alignment(t->io.mask);
    t->target.max_transfer=batch;
    if(!t->request || !t->buffer || !t->target.bounce ||
       dolly_dma_check_buffer((uintptr_t)t->target.bounce,batch,t->target.dma_mask,
                              t->target.dma_alignment)!=DOLLY_DMA_OK) {
        problem="Cannot allocate memory compatible with the destination DMA settings.";
        goto fail;
    }
    if(OpenDevice((STRPTR)t->device.driver,t->device.unit,(struct IORequest *)t->request,0)) {
        problem="Cannot open the destination for writing.";
        goto fail;
    }
    t->opened=1;
    t->target.request=t->request;
    t->own_opens=1u+(!restore && !strcasecmp(t->reader.device.driver,t->device.driver));
    if(!target_idle(t)) {
        problem="Destination driver is already in use. Take the target offline.";
        goto fail;
    }
    if(!known_driver && !dolly_amiga_device_advertises_write(t->request)) {
        problem="Destination has no qualified write profile or advertised WRITE/UPDATE support.";
        goto fail;
    }
    if(dolly_block_open_backend(t->target.capacity,read_target,keep_reader,t,
                                &locked_reader)!=DOLLY_BLOCK_OK) {
        problem="Cannot allocate the destination assessment reader.";
        goto fail;
    }
    if(!extent_anchors(input,0,t->target.length,t->target.sector_bytes,t->buffer,&first,&last) ||
       first!=t->source_first_crc || last!=t->source_last_crc ||
       !extent_anchors(locked_reader,t->target.start,t->target.length,t->target.sector_bytes,
                       t->buffer,&first,&last) ||
       first!=t->target_first_crc || last!=t->target_last_crc) {
        problem="Source or destination changed during confirmation.";
        goto fail;
    }
    if(restore) {
        DollyRestoreAssessment checked=assess_image(t,locked_reader,destination);
        if(checked.decision!=approval.decision || strcmp(checked.reason,approval.reason)) {
            problem="Image or destination assessment changed during confirmation.";
            goto fail;
        }
    }
    dolly_block_close(locked_reader);
    locked_reader=NULL;
    if(!target_idle(t)) {
        problem="Destination became busy during preflight.";
        goto fail;
    }
    t->request->io_Command=CMD_UPDATE;
    t->request->io_Flags=0;
    t->request->io_Error=0;
    t->request->io_Offset=0;
    t->request->io_Actual=0;
    t->request->io_Data=NULL;
    t->request->io_Length=0;
    if(DoIO((struct IORequest *)t->request) || t->request->io_Error) {
        problem="Destination driver refused CMD_UPDATE. No sectors were written.";
        goto fail;
    }
    if(dolly_restore_transfer_init(&t->job,input,t->target.length,t->target.sector_bytes,
                                   batch,t,write_extent,verify))return 1;
    problem="Cannot initialize the aligned transfer.";
fail:
    if(problem)dolly_amiga_role_request(parent,restore?"Dolly - Restore blocked":"Dolly - Copy blocked",
                                       problem,"OK",NULL,NULL);
cancel:
    dolly_block_close(locked_reader);
    t->failure=problem?"Transfer blocked.":"Transfer cancelled.";
    dolly_surface_close(&disk);
    dolly_amiga_transfer_close(t);
    return 0;
}

void dolly_amiga_transfer_step(DollyAmigaTransfer *t) {
    DollyRestoreTransferState state=dolly_restore_transfer_step(&t->job,t->buffer,t->target.bounce_bytes);
    if(state==DOLLY_RESTORE_READ_FAILED)t->failure=dolly_block_status_name(t->job.read_status);
    else if(state==DOLLY_RESTORE_WRITE_FAILED && !t->failure) {
        switch(t->job.write_status) {
        case DOLLY_WRITE_REJECTED:t->failure="Write rejected before submission.";break;
        case DOLLY_WRITE_FLUSH_FAILED:t->failure="Destination flush failed.";break;
        case DOLLY_WRITE_VERIFY_FAILED:t->failure="Destination readback failed.";break;
        case DOLLY_WRITE_MISMATCH:t->failure="Destination readback differs.";break;
        default:t->failure="Destination write failed or was short.";break;
        }
    }
}
