#include "dolly_amiga_transfer.h"
#include "dolly_restore_check.h"
#include "dolly_ffs_analyze.h"
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
        if(!source) {
            uint32_t n;
            BPTR lock;
            for(n=0;(lock=dolly_amiga_restore_volume(&t->image,n))!=0;++n)
                if(dolly_amiga_lock_uses_mount(lock,mount)) {
                    t->failure="An image part uses a filesystem that must be unmounted.";
                    return 0;
                }
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
    DollyWarnings *warnings;
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
    if(!extent_anchors(approval->input,0,
                       t->image.is_dly?disk->sector_bytes:dolly_block_size(approval->input),
                       disk->sector_bytes,disk->buffer,
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
    if(t->target.start+t->target.length>UINT64_C(0x100000000)) {
        const int legacy_scsi=!strcasecmp(t->device.driver,"scsi.device") &&
            t->request->io_Device->dd_Library.lib_Version<43u &&
            strncmp(t->device_name,"UAE-IDE ",8) && strcmp(t->device_name,"UAE-IDE");
        if((legacy_scsi && !dolly_amiga_legacy_override()) ||
           !dolly_amiga_device_advertises_nsd64_write(t->request)) {
            t->failure="Destination extends beyond 4 GiB and the driver has no qualified NSD READ64/WRITE64 support.";
            return -1;
        }
        t->target.nsd64=1;
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
    if(!dolly_warning_add(approval->warnings,"Driver access",message,"Proceed")) {
        t->failure="Cannot add the driver access warning. Transfer blocked.";
        return -1;
    }
    t->expected_opens=open_count;
    return 1;
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
    return dolly_amiga_write_target_read(&t->target,offset,buffer,bytes);
}

static int extent_anchors(DollyBlockSource *blocks,uint64_t start,uint64_t length,
                          uint32_t sector,void *buffer,uint32_t *first,uint32_t *last) {
    DollyChecksum checksum;
    if(dolly_block_read(blocks,start,buffer,sector).status!=DOLLY_BLOCK_OK)return 0;
    dolly_checksum_init(&checksum);
    dolly_checksum_update(&checksum,buffer,sector);
    *first=dolly_checksum_final(&checksum);
    if(length==sector) {
        *last=*first;
        return 1;
    }
    if(dolly_block_read(blocks,start+length-sector,buffer,sector).status!=DOLLY_BLOCK_OK)return 0;
    dolly_checksum_init(&checksum);
    dolly_checksum_update(&checksum,buffer,sector);
    *last=dolly_checksum_final(&checksum);
    return 1;
}

static int add_grow_metadata_plan(DollyWarnings *results,const DollyFfsReport *report) {
    char message[460],available[21],page_first[21],page_last[21],ext_first[21],ext_last[21];
    uint64_t first,last;
    uint64_t count=(uint64_t)report->grow_bitmap_pages+report->grow_bitmap_extensions;
    dolly_surface_log_decimal(available,report->grow_tail_available);
    if(!count)return 1;
    if(count>report->grow_tail_available) {
        char needed[21];
        dolly_surface_log_decimal(needed,count);
        snprintf(message,sizeof(message),
                 "The bitmap needs %s new metadata blocks. The added range has %s after "
                 "reserving the planned root. This layout needs space elsewhere.",
                 needed,available);
        return dolly_warning_add(results,"FFS bitmap placement blocked",message,"OK")!=NULL;
    }
    if(report->grow_bitmap_pages) {
        if(!dolly_ffs_grow_metadata_block(report,0,0,&first) ||
           !dolly_ffs_grow_metadata_block(report,0,report->grow_bitmap_pages-1u,&last))return 0;
        dolly_surface_log_decimal(page_first,first);
        dolly_surface_log_decimal(page_last,last);
        snprintf(message,sizeof(message),"New bitmap pages: blocks %s to %s in order.\n"
                 "The planned root block is excluded from the assignment.",page_first,page_last);
        if(!dolly_warning_add(results,"FFS bitmap page placement",message,"OK"))return 0;
    }
    if(report->grow_bitmap_extensions) {
        if(!dolly_ffs_grow_metadata_block(report,1,0,&first) ||
           !dolly_ffs_grow_metadata_block(report,1,report->grow_bitmap_extensions-1u,&last))return 0;
        dolly_surface_log_decimal(ext_first,first);
        dolly_surface_log_decimal(ext_last,last);
        snprintf(message,sizeof(message),"New bitmap extension blocks: %s to %s in order.\n"
                 "The planned root block is excluded from the assignment.",ext_first,ext_last);
        if(!dolly_warning_add(results,"FFS bitmap extension placement",message,"OK"))return 0;
    }
    return 1;
}

void dolly_amiga_analyze_copy(struct Window *parent,const DollyEndpoint *source,
                              const DollyEndpoint *destination,int smart_copy) {
    DollySurfaceScan *disk=NULL,*target=NULL;
    DollyWarnings warnings={0},results={0};
    DollyFfsReport report;
    uint64_t block_bytes;
    const char *problem=smart_copy?"Choose two partitions or two logical devices.":
                                    "Choose two in-range RDB partitions.";
    char message[360],total[21],used[21],highest[21],minimum[21],limit[21],outside[21];
    if(source->kind!=destination->kind ||
       (source->kind!=DOLLY_ENDPOINT_PARTITION &&
        !(smart_copy && source->kind==DOLLY_ENDPOINT_LOGICAL)) ||
       source->outside_range!=DOLLY_RANGE_STRICT ||
       destination->outside_range!=DOLLY_RANGE_STRICT)goto done;
    problem="Not enough memory for FFS analysis.";
    disk=calloc(1,sizeof(*disk));
    target=calloc(1,sizeof(*target));
    if(!disk || !target)goto done;
    if(!dolly_surface_open(disk,source,parent,DOLLY_IO_SOURCE,0,&warnings)) {
        problem=disk->failure;
        goto done;
    }
    if(!dolly_surface_open(target,destination,parent,DOLLY_IO_DESTINATION,0,&warnings)) {
        problem=target->failure;
        goto done;
    }
    problem=source->kind==DOLLY_ENDPOINT_LOGICAL?
        "Each logical device must match one in-range RDB partition on its driver and unit.":
        "FFS copy analysis requires two validated RDB partitions.";
    if(!disk->is_rdb_partition || !target->is_rdb_partition)goto done;
    problem="Source and destination overlap on the same device and unit.";
    if(!strcasecmp(disk->device.driver,target->device.driver) &&
       disk->device.unit==target->device.unit &&
       dolly_unit_relation(start_of(disk),length_of(disk),start_of(target),length_of(target))
           !=DOLLY_UNIT_RANGE_DISJOINT)goto done;
    block_bytes=(uint64_t)disk->rdb_size_block*disk->rdb_sectors_per_block;
    problem="Unsupported FFS source geometry or unaligned destination extent.";
    if(!disk->rdb_dos_type || block_bytes<512u || block_bytes>32768u ||
       (block_bytes&(block_bytes-1u)) || length_of(target)%block_bytes)goto done;
    if(smart_copy) {
        problem="Smart Copy requires source and destination partitions of equal size.";
        if(length_of(disk)!=length_of(target))goto done;
        problem="Smart Copy requires matching RDB filesystem block size, reserved blocks and DosType.";
        if(block_bytes!=(uint64_t)target->rdb_size_block*target->rdb_sectors_per_block ||
           disk->rdb_reserved_blocks!=target->rdb_reserved_blocks ||
           disk->rdb_dos_type!=target->rdb_dos_type)goto done;
    }
    /* The target is opened for read-only inspection. No target mount changes. */
    problem="Cannot prepare the source filesystem for bitmap analysis.";
    if(!dolly_amiga_guard_read(&disk->mount_guard,parent,&warnings,&disk->device,
                              start_of(disk),length_of(disk),0))goto done;
    if(!dolly_warnings_review(parent,&warnings)) {
        problem=NULL;
        goto done;
    }
    problem="The source filesystem could not be isolated for bitmap analysis.";
    if(!dolly_amiga_read_guard_isolated(disk->mount_guard))goto done;
    problem=dolly_ffs_analyze(disk->source,start_of(disk),length_of(disk),(uint32_t)block_bytes,
                              disk->rdb_reserved_blocks,disk->rdb_dos_type,length_of(target),&report);
    if(problem)goto done;
    if(!dolly_amiga_read_guard_isolated(disk->mount_guard)) {
        problem="The source filesystem changed during bitmap analysis.";
        goto done;
    }
    dolly_surface_log_decimal(total,report.blocks);
    dolly_surface_log_decimal(used,report.used);
    dolly_surface_log_decimal(highest,report.highest_used);
    dolly_surface_log_decimal(minimum,report.minimum_blocks);
    dolly_surface_log_decimal(limit,report.target_blocks);
    dolly_surface_log_decimal(outside,report.used_outside_target);
    problem="Cannot collect FFS analysis results.";
    snprintf(message,sizeof(message),
             "Read-only FFS bitmap analysis (DOS/%lu).\n"
             "Filesystem block: %lu bytes. Source: %s blocks, %s occupied.\n"
             "Highest occupied block: %s. Minimum in place: %s blocks.\n"
             "Destination: %s blocks. Occupied blocks outside it: %s.",
             (unsigned long)(report.dos_type&255u),(unsigned long)report.block_bytes,
             total,used,highest,minimum,limit,outside);
    if(!dolly_warning_add(&results,"FFS bitmap analysis",message,"OK"))goto done;

    if(smart_copy) {
        char skipped[21],ranges[21];
        dolly_surface_log_decimal(skipped,report.blocks-report.used);
        dolly_surface_log_decimal(ranges,report.copy_runs);
        snprintf(message,sizeof(message),
                 "Copy %s occupied or reserved blocks in %s contiguous ranges.\n"
                 "Skip %s free blocks. Keep the original partition-relative block positions.\n"
                 "Preview only. No data was written.",
                 used,ranges,skipped);
        if(!dolly_warning_add(&results,"Smart Copy plan",message,"OK"))goto done;
    } else if(report.grow_added_blocks) {
        static const char *const placements[]={
            "current root", "free in the source", "in the added range",
            "occupied by FFS metadata", "occupied by a data or directory block"
        };
        char added[21],root[21];
        dolly_surface_log_decimal(added,report.grow_added_blocks);
        dolly_surface_log_decimal(root,report.grow_root_block);
        snprintf(message,sizeof(message),
                 "Grow by %s blocks. Planned root: %s (%s).\n"
                 "Additional bitmap blocks: %lu. Extension blocks: %lu.\n"
                 "Actual resizing also needs filesystem metadata changes.",
                 added,root,placements[report.grow_root_placement],
                 (unsigned long)report.grow_bitmap_pages,
                 (unsigned long)report.grow_bitmap_extensions);
        if(!dolly_warning_add(&results,"FFS growth plan",message,"OK") ||
           !add_grow_metadata_plan(&results,&report))goto done;
        if(report.grow_root_placement<=DOLLY_FFS_ROOT_NEW_SPACE &&
           (uint64_t)report.grow_bitmap_pages+report.grow_bitmap_extensions<=report.grow_tail_available &&
           !dolly_warning_add(&results,"FFS bitmap preview",
               "Sample bitmap pages were generated in memory with new checksums. "
               "The old root remains allocated until references can be updated.","OK"))
            goto done;
    } else if(report.target_blocks<report.blocks) {
        if(report.used_outside_target) {
            char first[21];
            dolly_surface_log_decimal(first,report.first_outside_target);
            snprintf(message,sizeof(message),
                     "First occupied block outside destination: %s.\n"
                     "Occupied blocks outside the selected extent require relocation.",
                     first);
        } else {
            snprintf(message,sizeof(message),
                     "All occupied blocks fit inside the destination extent.\n"
                     "Actual resizing also needs filesystem metadata changes.");
        }
        if(!dolly_warning_add(&results,"FFS shrink assessment",message,"OK"))goto done;
    }

    if(block_bytes!=(uint64_t)target->rdb_size_block*target->rdb_sectors_per_block ||
       disk->rdb_reserved_blocks!=target->rdb_reserved_blocks ||
       disk->rdb_dos_type!=target->rdb_dos_type) {
        if(!dolly_warning_add(&results,"Destination RDB settings",
             "Destination FFS block size, reserved blocks or DosType differ from the source.",
             "OK"))goto done;
    }
    problem=NULL;
done:
    dolly_warnings_clear(&warnings);
    if(target) {
        dolly_surface_close(target);
        free(target);
    }
    if(disk) {
        dolly_surface_close(disk);
        free(disk);
    }
    if(problem) {
        dolly_warnings_clear(&results);
        if(!dolly_warning_add(&results,"FFS analysis blocked",problem,"OK"))
            dolly_amiga_role_request(parent,"Dolly - FFS analysis blocked",problem,"OK",NULL,NULL);
    }
    (void)dolly_warnings_review(parent,&results);
    dolly_warnings_clear(&results);
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
        problem="Choose a whole disk or an in-range RDB partition. Restore requires a RAW/HDF or IFF DOLY file.";
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
        DollyBlockStatus status=dolly_amiga_restore_source_open(&t->image,source,disk.sector_bytes,parent);
        if(status!=DOLLY_BLOCK_OK) {
            snprintf(reason,sizeof(reason),"Cannot open the image or split manifest. Check sector size and required parts: %s.",
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
        {
            uint32_t n=0;
            BPTR lock=t->image.lock;
            do {
                if(t->check_target_mounts) {
                    DollyPhysicalEntry image_device;
                    uint64_t file_start,file_length;
                    if(dolly_physical_from_lock(lock,&image_device,&file_start,&file_length)==1 &&
                       image_device.unit==t->device.unit &&
                       !strcasecmp(image_device.driver,t->device.driver)) {
                        problem="An image file is on the device that must be unmounted.";
                        goto fail;
                    }
                }
                problem=dolly_amiga_guard_file_transfer(&warnings,lock,&disk.device,
                                                         t->target.start,t->target.length);
                if(problem)goto fail;
                lock=dolly_amiga_restore_volume(&t->image,++n);
            } while(lock);
        }
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
    write_approval=(WriteApproval){t,&disk,input,&warnings,known_driver};
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
    /* The validation pass will read the full DOLY source, including its
     * last sector. Avoid indexing a large final part just to read its tail. */
    if(!extent_anchors(input,0,t->image.is_dly?t->target.sector_bytes:dolly_block_size(input),
                       t->target.sector_bytes,t->buffer,&first,&last) ||
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
                   t->target.sector_bytes,batch,t,write_extent,verify)) {
        if(t->image.is_dly)t->job.source_check_bytes=dolly_block_size(input);
        return 1;
    }
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
    if(state==DOLLY_RESTORE_READ_FAILED && !t->failure) {
        if(t->job.source_checked<t->job.source_check_bytes) {
            uint32_t parts,part=dolly_amiga_restore_failed_part(&t->image,&parts);
            if(part) {
                snprintf(t->validation_failure,sizeof(t->validation_failure),
                         "IFF DOLY part %lu/%lu failed validation. No sectors were written.",
                         (unsigned long)part,(unsigned long)parts);
                t->failure=t->validation_failure;
            } else t->failure="IFF DOLY validation failed. No sectors were written.";
        } else t->failure=dolly_block_status_name(t->job.read_status);
    }
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
