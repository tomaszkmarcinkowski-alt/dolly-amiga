#include "dolly_amiga_restore_confirm.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_io_settings.h"
#include "dolly_amiga_surface_scan.h"
#include "dolly_amiga_transfer_guard.h"
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

int dolly_amiga_confirm_restore_assessment(struct Window *parent,
                                           const DollyRestoreAssessment *assessment) {
    ULONG args[]={(ULONG)assessment->reason};
    struct EasyStruct request={
        sizeof(struct EasyStruct),0,(STRPTR)"Dolly - Restore blocked",
        (STRPTR)"%s",(STRPTR)"OK"
    };
    if(assessment->decision==DOLLY_RESTORE_MATCH)return 1;
    if(!parent)return 0;
    if(assessment->decision==DOLLY_RESTORE_MISMATCH && dolly_amiga_allow_restore_mismatch())
        return dolly_amiga_confirm_destructive(parent,assessment->reason);
    EasyRequestArgs(parent,&request,NULL,args);
    return 0;
}

int dolly_amiga_restore_preflight(struct Window *parent,
                       const DollyAmigaRestoreSource *image,
                       const DollyRestoreExtent *target) {
    DollyRestoreExtent source={image->blocks,0,dolly_block_size(image->blocks),
                               target->sector_bytes,DOLLY_RESTORE_UNKNOWN};
    DollyRestoreAssessment assessment=dolly_restore_assess(&source,target);
    return dolly_amiga_confirm_restore_assessment(parent,&assessment);
}

void dolly_amiga_restore_preview(struct Window *parent,const DollyEndpoint *image,
                                const DollyEndpoint *destination) {
    DollySurfaceScan disk={0};
    DollyAmigaRestoreSource source={0};
    DollyRestoreExtent target;
    DollyRestoreAssessment failure={DOLLY_RESTORE_BLOCKED,DOLLY_RESTORE_SIZE_EQUAL,""};
    DollyBlockStatus status;
    const char *problem;
    if(!dolly_surface_open(&disk,destination,parent,0)) {
        snprintf(failure.reason,sizeof(failure.reason),"Target: %s",
                 disk.failure?disk.failure:"Cannot read the destination.");
        dolly_amiga_confirm_restore_assessment(parent,&failure);
        goto out;
    }
    target=(DollyRestoreExtent){disk.source,disk.first_lba*disk.sector_bytes,
        disk.total*disk.sector_bytes,disk.sector_bytes,
        destination->kind==DOLLY_ENDPOINT_UNIT?DOLLY_RESTORE_RDB_DISK:DOLLY_RESTORE_PARTITION};
    status=dolly_amiga_restore_source_open(&source,image,target.sector_bytes);
    if(status!=DOLLY_BLOCK_OK) {
        snprintf(failure.reason,sizeof(failure.reason),"Cannot open a usable flat RAW/HDF image: %s.",
                 dolly_block_status_name(status));
        dolly_amiga_confirm_restore_assessment(parent,&failure);
        goto out;
    }
    if(!dolly_amiga_restore_preflight(parent,&source,&target))goto out;
    problem=dolly_amiga_guard_file_transfer(parent,source.lock,&disk.device,
                                           target.start,target.bytes);
    if(!problem) {
        struct EasyStruct request={
            sizeof(struct EasyStruct),0,(STRPTR)"Dolly - Restore preflight",
            (STRPTR)"Preflight completed. Restore writes are disabled in this build.",
            (STRPTR)"OK"
        };
        EasyRequestArgs(parent,&request,NULL,NULL);
    }
out:
    dolly_amiga_restore_source_close(&source);
    dolly_surface_close(&disk);
    dolly_surface_log_clear(&disk.log);
}

void dolly_amiga_copy_preview(struct Window *parent,const DollyEndpoint *source,
                              const DollyEndpoint *destination) {
    DollySurfaceScan src={0},dst={0};
    char reason[180],source_bytes[21],target_bytes[21];
    struct EasyStruct request={sizeof(struct EasyStruct),0,(STRPTR)"Dolly - Copy",
                               (STRPTR)"%s",(STRPTR)"OK"};
    ULONG args[]={(ULONG)reason};
    if(source->kind==DOLLY_ENDPOINT_FILE || destination->kind==DOLLY_ENDPOINT_FILE)
        strcpy(reason,"Copy preview currently supports device and partition extents.");
    else if(!dolly_surface_open(&src,source,parent,0))
        snprintf(reason,sizeof(reason),"Source: %s",src.failure?src.failure:"Cannot open source.");
    else if(!dolly_surface_open(&dst,destination,parent,0))
        snprintf(reason,sizeof(reason),"Destination: %s",dst.failure?dst.failure:"Cannot open destination.");
    else {
        uint64_t a=src.total*src.sector_bytes,b=dst.total*dst.sector_bytes;
        dolly_surface_log_decimal(source_bytes,a);
        dolly_surface_log_decimal(target_bytes,b);
        if(a!=b || src.sector_bytes!=dst.sector_bytes)
            snprintf(reason,sizeof(reason),
                "Copy blocked: source %s bytes, target %s bytes.\n"
                "Sector size: source %lu B, target %lu B.\n"
                "Copy requires matching size and sector geometry.",
                source_bytes,target_bytes,(unsigned long)src.sector_bytes,
                (unsigned long)dst.sector_bytes);
        else if(src.device.unit==dst.device.unit &&
                !strcasecmp(src.device.driver,dst.device.driver) &&
                dolly_unit_relation(src.first_lba,src.total,dst.first_lba,dst.total)
                    !=DOLLY_UNIT_RANGE_DISJOINT)
            strcpy(reason,"Copy blocked: source and destination ranges overlap.");
        else {
            if(src.device.unit==dst.device.unit &&
               !strcasecmp(src.device.driver,dst.device.driver))
                dolly_amiga_warn_same_unit(parent);
            snprintf(reason,sizeof(reason),"Copy into %s / Unit %lu.\n"
                "Target bytes: %s. This build runs a read-only preview.",
                dst.device.driver,(unsigned long)dst.device.unit,target_bytes);
            if(!dolly_amiga_confirm_destructive(parent,reason))goto out;
            strcpy(reason,"Copy preflight completed. Device writing is pending.");
        }
    }
    EasyRequestArgs(parent,&request,NULL,args);
out:
    dolly_surface_close(&dst);
    dolly_surface_close(&src);
    dolly_surface_log_clear(&dst.log);
    dolly_surface_log_clear(&src.log);
}
