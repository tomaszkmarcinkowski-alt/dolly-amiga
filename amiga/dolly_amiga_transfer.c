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
static int rdb_or_empty(const DollySurfaceScan *scan,const DollyEndpoint *endpoint,
                        const char *role,char *reason,size_t reason_size) {
    DollyNativePartitionMap *map=malloc(sizeof(*map));
    int ok=0;
    const char *detail="Cannot allocate partition scan.";
    if(!map)goto report;
    if(dolly_amiga_partition_scan(scan->device.driver,scan->device.unit,&scan->geometry,map)) {
        detail="Only validated RDB or unpartitioned 512-byte media are supported.";
        if(endpoint->kind==DOLLY_ENDPOINT_UNIT) {
            ok=map->format==DOLLY_NATIVE_PARTITION_RDB && !map->rdb_capacity_mismatch;
            if(map->rdb_capacity_mismatch)detail=map->status;
        } else if(endpoint->partition<map->count) {
            const DollyNativePartition *part=&map->parts[endpoint->partition];
            char driver[DOLLY_PHYS_DRIVER_LEN];
            ULONG unit;
            const char *tail=dolly_amiga_parse_unit(endpoint->path,driver,sizeof(driver),&unit);
            ok=tail && *tail && !strcmp(tail+3,part->label) &&
               part->is_rdb && !part->outside_media &&
               part->start_bytes==start_of(scan) && part->length_bytes==length_of(scan);
        }
    } else {
        ok=endpoint->kind==DOLLY_ENDPOINT_UNIT && map->no_partition_table;
        detail=map->status;
    }
report:
    if(!ok)snprintf(reason,reason_size,"%s: %.140s\n%s",role,endpoint->path,detail);
    free(map);
    return ok;
}
static void describe_partition_table(const DollySurfaceScan *scan,char out[110]) {
    DollyNativePartitionMap *map=malloc(sizeof(*map));
    if(!map) {
        strcpy(out,"Unknown (out of memory)");
        return;
    }
    dolly_amiga_partition_scan(scan->device.driver,scan->device.unit,&scan->geometry,map);
    if(map->format==DOLLY_NATIVE_PARTITION_RDB) {
        if(map->rdb_capacity_mismatch)snprintf(out,110,"%.109s",map->status);
        else strcpy(out,"RDB");
    }
    else if(map->format==DOLLY_NATIVE_PARTITION_MBR)
        strcpy(out,"MBR");
    else if(map->no_partition_table)
        strcpy(out,"No recognized partition table");
    else
        snprintf(out,110,"Unconfirmed: %.95s",map->status);
    free(map);
}

static int mounts_unchanged(DollyAmigaTransfer *t,int source);
static DollyBlockReadResult read_extent(void *context,uint64_t offset,
                                       void *data,size_t bytes) {
    DollyAmigaTransfer *transfer=context;
    DollySurfaceScan *source=&transfer->reader;
    if(!mounts_unchanged(transfer,1)) {
        transfer->failure="Source filesystem changed during read.";
        return (DollyBlockReadResult){DOLLY_BLOCK_EDEVICE,0};
    }
    return dolly_surface_read(source,start_of(source)+offset,data,bytes);
}
static void keep_reader(void *context) {
    (void)context; /* DollyAmigaTransfer owns the underlying reader. */
}
static DollyMountedVolume *find_mount(DollyMountList *list,const DollyMountedVolume *mount) {
    unsigned i;
    for(i=0;i<list->count;++i)
        if(list->entries[i].entry==mount->entry &&
           !strcasecmp(list->entries[i].device,mount->device))return &list->entries[i];
    return NULL;
}
static int mounts_unchanged(DollyAmigaTransfer *t,int source) {
    const DollyPhysicalEntry *device=source?&t->reader.device:&t->device;
    DollyMountList *recorded=source?&t->source_mounts:&t->target_mounts;
    uint64_t start=source?start_of(&t->reader):0;
    uint64_t length=source?length_of(&t->reader):t->target.capacity;
    unsigned i;
    if(!(source?t->check_source_mounts:t->check_target_mounts))return 1;
    if(!dolly_amiga_mounted_extents(device,start,length,!source,&t->observed_mounts))return 0;
    for(i=0;i<t->observed_mounts.count;++i) {
        DollyMountedVolume *current=&t->observed_mounts.entries[i];
        DollyMountedVolume *old=find_mount(recorded,current);
        if(source) {
            DollyMountedVolume *shared=find_mount(&t->target_mounts,current);
            if(shared)old=shared;
        }
        if(!old || old->handler!=current->handler ||
           ((!source || current->volume[0]) &&
            (!old->allow_present || strcmp(old->volume,current->volume))))return 0;
    }
    return 1;
}
static int apply_mount_warning(DollyWarning *warning) {
    DollyAmigaTransfer *t=warning->context;
    int source=warning->index>=DOLLY_MOUNT_MAX;
    const DollyPhysicalEntry *device=source?&t->reader.device:&t->device;
    DollyMountList *list=source?&t->source_mounts:&t->target_mounts;
    uint64_t start=source?start_of(&t->reader):0;
    uint64_t length=source?length_of(&t->reader):t->target.capacity;
    DollyMountedVolume *mount=&list->entries[warning->index%DOLLY_MOUNT_MAX];
    const DollyMountedVolume *current;
    char message[300];
    if(!dolly_amiga_mounted_extents(device,start,length,!source,&t->observed_mounts))
        return -1;
    current=find_mount(&t->observed_mounts,mount);
    if(!current || current->handler!=mount->handler || strcmp(current->volume,mount->volume)) {
        t->failure="A mount changed. Start the operation again.";
        return -1;
    }
    if(mount->handler)mount->suspended=dolly_amiga_inhibit_mount(mount,1);
    if(!source && (!mount->handler || mount->suspended) && dolly_amiga_unmount(mount)) {
        mount->suspended=0;
        return 1;
    }
    if(!dolly_amiga_mounted_extents(device,start,length,!source,&t->observed_mounts))
        return -1;
    current=find_mount(&t->observed_mounts,mount);
    if(source && mount->suspended && (!current || !current->volume[0]))return 1;
    if(!source && current) {
        int suspended=mount->suspended && mount->handler==current->handler;
        *mount=*current;
        mount->suspended=suspended;
    }
    snprintf(message,sizeof(message),"%s: \"%s\"\nVolume: \"%s\"\n"
             "Dolly could not confirm that the filesystem was taken offline. "
             "Continuing while it is active can corrupt data. Proceed at your own risk.",
             source?"Source":"Destination",mount->device,
             mount->volume[0]?mount->volume:"name unavailable");
    /* Cancellation rolls back source inhibits. The transfer still checks
     * the recorded handler and volume before it writes. */
    mount->allow_present=1;
    return dolly_warning_risk(warning,message);
}
static int collect_mount_warnings(DollyAmigaTransfer *t,DollyWarnings *warnings,int source) {
    const DollyPhysicalEntry *device=source?&t->reader.device:&t->device;
    DollyMountList *list=source?&t->source_mounts:&t->target_mounts;
    uint64_t start=source?start_of(&t->reader):0;
    uint64_t length=source?length_of(&t->reader):t->target.capacity;
    unsigned i;
    if(!(source?t->check_source_mounts:t->check_target_mounts))return 1;
    if(!dolly_amiga_mounted_extents(device,start,length,!source,list))return 0;
    for(i=0;i<list->count;++i) {
        DollyMountedVolume *mount=&list->entries[i];
        DollyWarning *warning;
        char message[250],title[46];
        if(source && find_mount(&t->target_mounts,mount))continue;
        if(!source && dolly_amiga_lock_uses_mount(t->image.lock,mount)) {
            t->failure="The image file uses a filesystem that must be unmounted.";
            return 0;
        }
        snprintf(title,sizeof(title),"%s %.29s",source?"Source":"Destination",mount->device);
        snprintf(message,sizeof(message),"%s: \"%s\"\nMounted volume: \"%s\"\n%s",
                 source?"Source":"Destination",mount->device,
                 mount->volume[0]?mount->volume:"name unavailable",
                 source?"Temporarily suspend access to this filesystem?":
                 "Inhibit and unmount this partition? A reboot may be needed even if you cancel.");
        warning=dolly_warning_add(warnings,title,message,source?"Suspend":"Unmount");
        if(!warning)return 0;
        warning->action=apply_mount_warning;
        warning->context=t;
        warning->index=i+(source?DOLLY_MOUNT_MAX:0);
    }
    return 1;
}

typedef struct WriteApproval {
    DollyAmigaTransfer *transfer;
    DollySurfaceScan *disk;
    DollyBlockSource *input;
    int known_driver;
} WriteApproval;

static int extent_anchors(DollyBlockSource *blocks,uint64_t start,uint64_t length,
                          uint32_t sector,void *buffer,uint32_t *first,uint32_t *last);

static int approve_write_warning(DollyWarning *warning) {
    WriteApproval *approval=warning->context;
    DollyAmigaTransfer *t=approval->transfer;
    DollySurfaceScan *disk=approval->disk;
    unsigned open_count;
    char message[330];
    if(!extent_anchors(approval->input,0,dolly_block_size(approval->input),disk->sector_bytes,disk->buffer,
                       &t->source_first_crc,&t->source_last_crc) ||
       !extent_anchors(disk->source,t->target.start,t->target.length,disk->sector_bytes,
                       disk->buffer,&t->target_first_crc,&t->target_last_crc)) {
        t->failure="Could not read both extent anchors.";
        return -1;
    }
    dolly_surface_close(disk);
    if(OpenDevice((STRPTR)t->device.driver,t->device.unit,(struct IORequest *)t->request,0)) {
        t->failure="Cannot open the destination for writing.";
        return -1;
    }
    t->opened=1;
    t->target.request=t->request;
    if(!t->request->io_Device ||
       (!approval->known_driver && !dolly_amiga_device_advertises_write(t->request))) {
        t->failure="No qualified driver write or flush support.";
        return -1;
    }
    open_count=t->request->io_Device->dd_Library.lib_OpenCnt;
    if(open_count==t->expected_opens)return 1;
    if(open_count<t->expected_opens) {
        t->failure="Destination driver open count is below the expected minimum. Transfer blocked.";
        return -1;
    }
    /* Exec exposes a count for the entire device driver. Unrelated units
     * can have open handles even when the selected unit is offline. */
    snprintf(message,sizeof(message),"Destination: \"%s / Unit %lu\"\n"
             "Driver-wide open count: %u. Dolly expected %u of its own. "
             "Other handles can belong to any unit. Dolly cannot confirm exclusive "
             "access to this unit. Proceed at your own risk.",
             t->device.driver,(unsigned long)t->device.unit,open_count,t->expected_opens);
    t->expected_opens=open_count;
    return dolly_warning_risk(warning,message);
}
static void release_inhibited_mounts(DollyAmigaTransfer *t,DollyMountList *list,int keep_inhibited) {
    unsigned i;
    for(i=0;i<list->count;++i) {
        DollyMountedVolume *mount=&list->entries[i];
        if(keep_inhibited && mount->suspended) {
            dolly_amiga_role_request(t->parent,"Dolly - Destination inhibited",
                "Restart to reload the inhibited destination filesystem safely.","OK",NULL,NULL);
            break;
        }
        if(mount->suspended && !dolly_amiga_inhibit_mount(mount,0)) {
            char message[120];
            snprintf(message,sizeof(message),"Could not re-enable \"%s\".",mount->device);
            dolly_amiga_role_request(t->parent,"Dolly - Filesystem still dismounted",
                                    message,"OK",NULL,NULL);
        }
    }
    list->count=0;
}
int dolly_amiga_prepare_rdb_unmount(struct Window *parent,const char *driver,ULONG unit,
                 uint64_t capacity,BPTR image_lock,BPTR recovery_drawer) {
    DollyWarnings warnings={0};
    DollyAmigaTransfer *t;
    unsigned i;
    int ok=0;
    if(!dolly_amiga_probe_rom_scsi(unit)) {
        dolly_amiga_role_request(parent,"Dolly - Restore RDB",
            "The selected driver does not match the required ROM scsi.device.","OK",NULL,NULL);
        return 0;
    }
    t=calloc(1,sizeof(*t));
    if(!t)return 0;
    t->parent=parent;
    t->check_target_mounts=1;
    t->image.lock=image_lock;
    snprintf(t->device.driver,sizeof(t->device.driver),"%s",driver);
    t->device.unit=unit;
    t->target.capacity=capacity;
    if(!collect_mount_warnings(t,&warnings,0))goto out;
    for(i=0;i<t->target_mounts.count;++i) {
        if(dolly_amiga_lock_uses_mount(recovery_drawer,&t->target_mounts.entries[i])) {
            dolly_amiga_role_request(parent,"Dolly - Restore RDB",
                "The recovery copy must be saved on a different unit.","OK",NULL,NULL);
            goto out;
        }
    }
    ok=dolly_warnings_review(parent,&warnings) &&
       dolly_amiga_unit_registered(driver,unit)==0;
out:
    dolly_warnings_clear(&warnings);
    release_inhibited_mounts(t,&t->target_mounts,0);
    free(t);
    return ok;
}

static int target_idle(DollyAmigaTransfer *t) {
    return t->request->io_Device &&
           t->request->io_Device->dd_Library.lib_OpenCnt==t->expected_opens &&
           mounts_unchanged(t,0) && mounts_unchanged(t,1);
}
static DollyBlockWriteStatus write_extent(void *context,uint64_t offset,
                                      const void *data,size_t bytes,int verify) {
    DollyAmigaTransfer *t=context;
    if(!target_idle(t)) {
        t->failure="Destination became busy. Write stopped.";
        return DOLLY_WRITE_REJECTED;
    }
    {
        DollyBlockWriteStatus status=dolly_amiga_write_batch(&t->target,offset,data,bytes,verify);
        if(status!=DOLLY_WRITE_REJECTED)t->may_have_written=1;
        return status;
    }
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
    release_inhibited_mounts(t,&t->source_mounts,0);
    release_inhibited_mounts(t,&t->target_mounts,t->may_have_written);
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
    DollyWarnings warnings={0};
    DollyWarning *write_warning;
    WriteApproval write_approval;
    DollyBlockSource *input=NULL,*locked_reader=NULL;
    DollyRestoreAssessment approval={0};
    DollyMediaGeometry live={0};
    char live_name[112],size[40],reason[700],a[21],b[21];
    char table[110],live_table[110];
    const char *problem=NULL;
    ULONG batch,verify=dolly_amiga_verify_writes();
    uint32_t first,last;
    uint64_t transfer_bytes;
    int known_driver=0;
    int whole_restore=restore && destination->kind==DOLLY_ENDPOINT_UNIT;
    int whole_copy=!restore && source->kind==DOLLY_ENDPOINT_UNIT &&
                   destination->kind==DOLLY_ENDPOINT_UNIT;
    dolly_amiga_transfer_close(t);
    memset(t,0,sizeof(*t));
    t->restoring=restore;
    t->parent=parent;
    if(!raw_endpoint(destination) || destination->outside_range!=DOLLY_RANGE_STRICT ||
       (!restore && (!raw_endpoint(source) || source->outside_range!=DOLLY_RANGE_STRICT))) {
        problem="Choose a whole disk or an in-range RDB partition. Restore requires a RAW/HDF file.";
        goto fail;
    }
    if(!restore && !dolly_surface_open(&t->reader,source,parent,DOLLY_IO_SOURCE,0,&warnings)) {
        problem=t->reader.failure;
        goto fail;
    }
    if(!dolly_surface_open(&disk,destination,parent,DOLLY_IO_DESTINATION,0,&warnings)) {
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
    if(whole_copy || whole_restore)describe_partition_table(&disk,table);
    if(!whole_copy && !whole_restore &&
       (!rdb_or_empty(&disk,destination,"Destination",reason,sizeof(reason)) ||
       (!restore && !rdb_or_empty(&t->reader,source,"Source",reason,sizeof(reason))))) {
        problem=reason;
        goto fail;
    }
    if(!strcasecmp(t->device.driver,"scsi.device"))
        known_driver=!strncmp(t->device_name,"UAE-IDE ",8) ||
                     !strcmp(t->device_name,"UAE-IDE") || dolly_amiga_probe_rom_scsi(t->device.unit);
    t->check_target_mounts=1;
    t->check_source_mounts=!restore;
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
        if(approval.decision==DOLLY_RESTORE_BLOCKED) {
            problem=approval.reason;
            goto fail;
        }
        if(approval.size!=DOLLY_RESTORE_SIZE_EQUAL) {
            uint64_t image_bytes=dolly_block_size(t->image.blocks);
            int smaller=approval.size==DOLLY_RESTORE_SIZE_SMALLER;
            char difference[21];
            dolly_surface_log_decimal(a,image_bytes);
            dolly_surface_log_decimal(b,t->target.length);
            dolly_surface_log_decimal(difference,smaller?t->target.length-image_bytes:
                                                        image_bytes-t->target.length);
            snprintf(reason,sizeof(reason),"Image: %s bytes. Destination: %s bytes. Difference: %s bytes.\n%s",
                     a,b,difference,smaller?
                     "Dolly writes the entire image and leaves the remaining destination sectors untouched. "
                     "The restored layout may leave that extra space unusable. No partition or filesystem is expanded.":
                     "Dolly writes only the part that fits. The end of the image will be lost. "
                     "Partitions or filesystems may be incomplete, corrupt or unreadable.");
            if(!dolly_warning_add(&warnings,smaller?"Image smaller than destination":
                                "Image larger than destination",reason,"Accept size difference")) {
                problem="Cannot collect Restore size warning.";
                goto fail;
            }
        }
        if(approval.decision==DOLLY_RESTORE_MISMATCH) {
            snprintf(reason,sizeof(reason),"%s\nProceed at your own risk.",approval.reason);
            if(!dolly_warning_add(&warnings,"Restore compatibility",reason,"Proceed")) {
                problem="Cannot collect Restore compatibility warning.";
                goto fail;
            }
        }
        if(t->check_target_mounts) {
            DollyPhysicalEntry image_device;
            uint64_t file_start,file_length;
            if(dolly_physical_from_lock(t->image.lock,&image_device,&file_start,&file_length)==1 &&
               image_device.unit==t->device.unit &&
               !strcasecmp(image_device.driver,t->device.driver)) {
                problem="The image is on the device that must be unmounted. Move it to another device.";
                goto fail;
            }
        }
        problem=dolly_amiga_guard_file_transfer(&warnings,t->image.lock,&disk.device,
                                                 t->target.start,t->target.length);
        if(problem)goto fail;
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
            if(t->check_target_mounts &&
               (!dolly_amiga_mounted_extents(&t->reader.device,start_of(&t->reader),
                    length_of(&t->reader),0,&t->observed_mounts) || t->observed_mounts.count)) {
                problem="Source must be offline for copying within this unit. "
                        "All partitions on the destination unit must be unmounted.";
                goto fail;
            }
            if(!dolly_amiga_warn_same_unit(&warnings)) {
                problem="Cannot collect same-unit warning.";
                goto fail;
            }
        }
        if(batch>t->reader.transfer_bytes)batch=t->reader.transfer_bytes;
        if(dolly_block_open_backend(length_of(&t->reader),read_extent,keep_reader,
                                    t,&t->copy_extent)!=DOLLY_BLOCK_OK) {
            problem="Cannot allocate the copy source.";
            goto fail;
        }
        input=t->copy_extent;
    }

    dolly_surface_log_decimal(a,t->target.start/disk.sector_bytes);
    transfer_bytes=dolly_block_size(input);
    if(transfer_bytes>t->target.length)transfer_bytes=t->target.length;
    dolly_surface_log_decimal(b,transfer_bytes/disk.sector_bytes);
    if(whole_copy || whole_restore)
        snprintf(reason,sizeof(reason),"%s of entire disk\nSource: \"%.135s\"\n"
                 "Destination: \"%.135s\"\n"
                 "Partition table on the disk to be overwritten: %s.\n"
                 "Write %s sectors starting at LBA 0. Verification: %s.",
                 restore?"Restore image":"Sector copy",source->path,destination->path,table,b,verify?"on":"off");
    else
        snprintf(reason,sizeof(reason),"%s\nSource: %.160s\nDestination: %.160s\n"
                 "Write %s sectors starting at LBA %s. Verification: %s.",
                 restore?"Restore image":"Sector copy",source->path,destination->path,b,a,
                 verify?"on":"off");
    if(!collect_mount_warnings(t,&warnings,0) || !collect_mount_warnings(t,&warnings,1)) {
        problem=t->failure?t->failure:"Cannot collect warnings about mounted filesystems.";
        goto fail;
    }
    {
        size_t used=strlen(reason);
        snprintf(reason+used,sizeof(reason)-used,
                 "\nBack up the destination first. Incorrect use can cause data loss.");
    }
    write_warning=dolly_warning_add(&warnings,"Overwrite destination",reason,"Proceed");
    if(!write_warning) {
        problem="Cannot collect destination overwrite warning.";
        goto fail;
    }
    write_warning->action=approve_write_warning;
    write_approval=(WriteApproval){t,&disk,input,known_driver};
    write_warning->context=&write_approval;

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
    t->expected_opens=1u+(!restore && !strcasecmp(t->reader.device.driver,t->device.driver));
    if(!dolly_warnings_review(parent,&warnings))goto cancel;
    dolly_warnings_clear(&warnings);

    /* Recheck the selected geometry and partition after the warning review.
     * The final approval closed the temporary reader. */
    if(!dolly_amiga_device_query(t->device.driver,t->device.unit,live_name,sizeof(live_name),
                                 size,sizeof(size),&live,1,NULL,0) ||
       !live.capacity_valid || strcmp(live_name,t->device_name) ||
       live.block_count!=t->geometry.block_count || live.block_bytes!=t->geometry.block_bytes ||
       live.cylinders!=t->geometry.cylinders || live.heads!=t->geometry.heads ||
       live.sectors_per_track!=t->geometry.sectors_per_track) {
        problem="Media geometry or partition selection changed. Query and select it again.";
        goto fail;
    }
    if(whole_copy || whole_restore) {
        describe_partition_table(&disk,live_table);
        if(strcmp(table,live_table)) {
            problem="Destination partition table changed during confirmation. Start again.";
            goto fail;
        }
    }
    if(!whole_copy && !whole_restore &&
       (!rdb_or_empty(&disk,destination,"Destination",reason,sizeof(reason)) ||
       (!restore && !rdb_or_empty(&t->reader,source,"Source",reason,sizeof(reason))))) {
        problem=reason;
        goto fail;
    }
    if(!target_idle(t)) {
        problem="A mounted filesystem or unexpected device access was detected.";
        goto fail;
    }
    if(dolly_block_open_backend(t->target.capacity,read_target,keep_reader,t,
                                &locked_reader)!=DOLLY_BLOCK_OK) {
        problem="Cannot allocate the destination assessment reader.";
        goto fail;
    }
    if(!extent_anchors(input,0,dolly_block_size(input),t->target.sector_bytes,t->buffer,&first,&last) ||
       first!=t->source_first_crc || last!=t->source_last_crc ||
       !extent_anchors(locked_reader,t->target.start,t->target.length,t->target.sector_bytes,
                       t->buffer,&first,&last) ||
       first!=t->target_first_crc || last!=t->target_last_crc) {
        problem="Source or destination changed during confirmation.";
        goto fail;
    }
    if(restore) {
        DollyRestoreAssessment checked=assess_image(t,locked_reader,destination);
        if(checked.decision!=approval.decision || checked.size!=approval.size ||
           strcmp(checked.reason,approval.reason)) {
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
    if(restore?dolly_restore_transfer_init_prefix(&t->job,input,t->target.length,
                   t->target.sector_bytes,batch,t,write_extent,verify):
               dolly_restore_transfer_init(&t->job,input,t->target.length,
                   t->target.sector_bytes,batch,t,write_extent,verify))return 1;
    problem="Cannot initialize the aligned transfer.";
fail:
    if(problem)dolly_amiga_role_request(parent,restore?"Dolly - Restore blocked":"Dolly - Copy blocked",
                                       problem,"OK",NULL,NULL);
cancel:
    dolly_warnings_clear(&warnings);
    dolly_block_close(locked_reader);
    if(!t->failure)t->failure=problem?"Transfer blocked.":"Transfer cancelled.";
    dolly_surface_close(&disk);
    dolly_amiga_transfer_close(t);
    return 0;
}

void dolly_amiga_transfer_step(DollyAmigaTransfer *t) {
    DollyRestoreTransferState state=dolly_restore_transfer_step(&t->job,t->buffer,t->target.bounce_bytes);
    if(state==DOLLY_RESTORE_READ_FAILED && !t->failure)t->failure=dolly_block_status_name(t->job.read_status);
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
