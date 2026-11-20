#include "dolly_restore_check.h"
#include "dolly_rdb.h"
#include "dolly_restore_source.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct RestoreInfo {
    DollyRestoreKind kind;
    DollyRdb *rdb;
    uint32_t fs;
    int unavailable,encoded,extent_overrun;
} RestoreInfo;

static DollyBlockReadResult read_span(void *context,uint64_t offset,void *data,
                                      size_t bytes) {
    const DollyRestoreExtent *span=context;
    return dolly_block_read(span->blocks,span->start+offset,data,bytes);
}
static void close_span(void *context) { (void)context; }

static const char *inspect(const DollyRestoreExtent *span,RestoreInfo *info,int whole_disk) {
    DollyBlockSource *view;
    DollyRdbStatus rdb_status;
    unsigned char boot[512],*sector;
    DollyBlockReadResult read={DOLLY_BLOCK_EIO,0};
    if(dolly_block_open_backend(span->bytes,read_span,close_span,(void *)span,&view)
       !=DOLLY_BLOCK_OK) {
        info->unavailable=1;
        return "Cannot allocate an inspection view.";
    }
    rdb_status=whole_disk?dolly_rdb_scan_extents(view,span->sector_bytes,16,info->rdb):
                         dolly_rdb_scan(view,0,span->sector_bytes,16,info->rdb);
    if(rdb_status==DOLLY_RDB_NOT_FOUND) {
        sector=malloc(span->sector_bytes);
        if(!sector) {
            dolly_block_close(view);
            info->unavailable=1;
            return "Cannot allocate a boot sector buffer.";
        }
        read=dolly_block_read(view,0,sector,span->sector_bytes);
        if(read.status==DOLLY_BLOCK_OK && read.bytes_read==span->sector_bytes)
            memcpy(boot,sector,sizeof(boot));
        free(sector);
    }
    dolly_block_close(view);
    if(rdb_status==DOLLY_RDB_OK) {
        unsigned i;
        for(i=0;i<info->rdb->partition_count;++i) {
            const DollyPartition *part=&info->rdb->partitions[i];
            if(part->start_bytes+part->length_bytes>span->bytes)
                info->extent_overrun=1;
        }
        info->kind=DOLLY_RESTORE_RDB_DISK;
        return NULL;
    }
    if(rdb_status!=DOLLY_RDB_NOT_FOUND) {
        info->unavailable=rdb_status==DOLLY_RDB_READ_ERROR;
        return dolly_rdb_status_name(rdb_status);
    }
    if(read.status!=DOLLY_BLOCK_OK || read.bytes_read!=span->sector_bytes) {
        info->unavailable=1;
        return "Cannot read the boot sector.";
    }
    if(!memcmp(boot,"FORM",4) && !memcmp(boot+8,"DOLY",4)) {
        info->encoded=1;
        return "IFF DOLY container must be decoded before assessment.";
    }
    if(boot[510]==0x55 && boot[511]==0xaa)
        return "MBR or PC boot signature detected. Partition layout was not verified.";
    if((memcmp(boot,"DOS",3)==0 && boot[3]<=7) ||
       (memcmp(boot,"PFS",3)==0 && (boot[3]==1 || boot[3]==2 || boot[3]==3)) ||
       (memcmp(boot,"SFS",3)==0 && (boot[3]==0 || boot[3]==2))) {
        info->kind=DOLLY_RESTORE_PARTITION;
        info->fs=(uint32_t)boot[0]<<24 | (uint32_t)boot[1]<<16 |
                 (uint32_t)boot[2]<<8 | boot[3];
        return NULL;
    }
    return "No supported RDB or filesystem signature recognized.";
}

static int valid_span(const DollyRestoreExtent *span) {
    uint64_t capacity=dolly_block_size(span->blocks);
    return dolly_restore_check_raw(span->bytes,span->bytes,span->sector_bytes)
               ==DOLLY_BLOCK_OK &&
           span->start%span->sector_bytes==0 && span->start<=capacity &&
           span->bytes<=capacity-span->start;
}
static const char *kind_name(DollyRestoreKind kind) {
    return kind==DOLLY_RESTORE_PARTITION?"partition":"whole RDB disk";
}

DollyRestoreAssessment dolly_restore_assess(const DollyRestoreExtent *image,
                                             const DollyRestoreExtent *target) {
    DollyRestoreAssessment result={DOLLY_RESTORE_BLOCKED,DOLLY_RESTORE_SIZE_EQUAL,""};
    RestoreInfo source={0},dest={0};
    const char *source_error,*dest_error;
    unsigned i;
    if(!image || !target) {
        strcpy(result.reason,"Image or target extent is unavailable.");
        return result;
    }
    if(!valid_span(image) || !valid_span(target) ||
       image->sector_bytes!=target->sector_bytes) {
        snprintf(result.reason,sizeof(result.reason),
                 "Invalid or unequal sector geometry. Image %lu, target %lu bytes per sector.",
                 (unsigned long)image->sector_bytes,(unsigned long)target->sector_bytes);
        return result;
    }
    if(image->bytes<target->bytes)result.size=DOLLY_RESTORE_SIZE_SMALLER;
    else if(image->bytes>target->bytes)result.size=DOLLY_RESTORE_SIZE_LARGER;
    source.rdb=malloc(sizeof(*source.rdb));
    dest.rdb=malloc(sizeof(*dest.rdb));
    if(!source.rdb || !dest.rdb) {
        strcpy(result.reason,"Not enough memory to inspect source and target.");
        goto done;
    }
    source_error=inspect(image,&source,target->expected_kind==DOLLY_RESTORE_RDB_DISK);
    dest_error=inspect(target,&dest,target->expected_kind==DOLLY_RESTORE_RDB_DISK);
    result.decision=source.unavailable || dest.unavailable || source.encoded?
                    DOLLY_RESTORE_BLOCKED:DOLLY_RESTORE_MISMATCH;
    if(dest.unavailable && !source.unavailable && !source.encoded)
        snprintf(result.reason,sizeof(result.reason),"Target: %s",dest_error);
    else if(source_error)
        snprintf(result.reason,sizeof(result.reason),"Image: %s",source_error);
    else if(dest_error)
        snprintf(result.reason,sizeof(result.reason),"Target: %s",dest_error);
    else if(target->expected_kind!=DOLLY_RESTORE_UNKNOWN &&
            dest.kind!=target->expected_kind)
        snprintf(result.reason,sizeof(result.reason),"Selected target is a %s. Its data identifies a %s.",
                 kind_name(target->expected_kind),kind_name(dest.kind));
    else if(source.kind!=dest.kind)
        snprintf(result.reason,sizeof(result.reason),"Image is a %s. Target is a %s.",
                 kind_name(source.kind),kind_name(dest.kind));
    else if(source.kind==DOLLY_RESTORE_PARTITION && source.fs!=dest.fs)
        snprintf(result.reason,sizeof(result.reason),"Filesystem ID differs. Image %08lX, target %08lX.",
                 (unsigned long)source.fs,(unsigned long)dest.fs);
    else if(source.kind==DOLLY_RESTORE_RDB_DISK) {
        const DollyRdb *a=source.rdb,*b=dest.rdb;
        if(a->cylinders!=b->cylinders || a->heads!=b->heads || a->sectors!=b->sectors)
            snprintf(result.reason,sizeof(result.reason),
                "RDB geometry differs. Image %lu/%lu/%lu, target %lu/%lu/%lu.",
                (unsigned long)a->cylinders,(unsigned long)a->heads,(unsigned long)a->sectors,
                (unsigned long)b->cylinders,(unsigned long)b->heads,(unsigned long)b->sectors);
        else if(a->partition_count!=b->partition_count)
            snprintf(result.reason,sizeof(result.reason),"RDB partition count differs. Image %u, target %u.",
                     a->partition_count,b->partition_count);
        else for(i=0;i<a->partition_count;++i) {
            const DollyPartition *x=&a->partitions[i],*y=&b->partitions[i];
            const char *field=NULL;
            if(x->start_bytes!=y->start_bytes || x->length_bytes!=y->length_bytes)
                field="extent";
            else if(!x->dos_type_present || !y->dos_type_present || x->dos_type!=y->dos_type)
                field="filesystem ID";
            else if(x->block_size_longwords!=y->block_size_longwords ||
                    x->sectors_per_block!=y->sectors_per_block ||
                    x->reserved_blocks!=y->reserved_blocks ||
                    x->preallocated_blocks!=y->preallocated_blocks)
                field="filesystem geometry";
            else if(strcmp(x->name,y->name)!=0)
                field="partition name";
            else if(x->max_transfer_present!=y->max_transfer_present ||
                    x->max_transfer_bytes!=y->max_transfer_bytes ||
                    x->buffer_memory_type_present!=y->buffer_memory_type_present ||
                    x->buffer_memory_type!=y->buffer_memory_type ||
                    x->dma_mask_present!=y->dma_mask_present || x->dma_mask!=y->dma_mask)
                field="I/O attributes";
            else if(x->boot_priority_present!=y->boot_priority_present ||
                    x->boot_priority!=y->boot_priority)
                field="boot priority";
            if(field) {
                snprintf(result.reason,sizeof(result.reason),"RDB partition %u: %s differs or is unknown.",
                         i+1,field);
                break;
            }
        }
    }
    if(result.decision!=DOLLY_RESTORE_BLOCKED &&
       (source.extent_overrun || dest.extent_overrun)) {
        size_t used=strlen(result.reason);
        snprintf(result.reason+used,sizeof(result.reason)-used,"%s%s RDB extends beyond capacity.",
                 used?"\n":"",source.extent_overrun?
                 (dest.extent_overrun?"Image and destination":"Image"):"Destination");
    }
    if(!result.reason[0])result.decision=DOLLY_RESTORE_MATCH;
done:
    free(source.rdb);
    free(dest.rdb);
    return result;
}
