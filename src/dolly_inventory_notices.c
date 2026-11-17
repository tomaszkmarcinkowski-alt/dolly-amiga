#include "dolly_inventory_notices.h"
#include "dolly_maxtransfer.h"
#include "dolly_rdb_audit.h"
#include <string.h>
/* A bounded, allocation-free lookup; the advice is deliberately
 * non-destructive and never recommends guessing storage parameters. */
static const char *suggestion_for(const char *code){
    if(!strcmp(code,"MAXTRANSFER_MISSING"))
        return "Check the active MountList/DosList value before drawing conclusions.";
    if(!strcmp(code,"MAXTRANSFER_ZERO"))
        return "Compare this PART entry with active mount settings and controller documentation.";
    if(!strcmp(code,"MAXTRANSFER_ABOVE_REFERENCE"))
        return "Identify the controller and driver; verify their documented transfer limit before making changes.";
    if(!strcmp(code,"MAXTRANSFER_BELOW_REFERENCE"))
        return "Check controller requirements; a smaller value may be intentional, do not raise it automatically.";
    if(!strcmp(code,"SECTORS_PER_BLOCK_NONSTANDARD"))
        return "Verify the original RDB and filesystem geometry; do not rewrite metadata based on this warning alone.";
    if(!strcmp(code,"MEMORY_MASK_ZERO"))
        return "Compare the PART mask with the active mount settings and controller DMA requirements.";
    if(!strcmp(code,"DOSTYPE_MISSING"))
        return "Inspect the original partition format and active mount configuration without modifying the disk.";
    if(!strcmp(code,"RDB_PARTITIONS_OVERLAP"))
        return "Review both byte/LBA ranges and keep the drive read-only until the layout is understood.";
    if(!strcmp(code,"RDB_HEADER_COLLISION"))
        return "Inspect the RDSK location and partition extent; do not attempt a repair without a verified backup.";
    if(!strcmp(code,"RDB_PART_DESCRIPTOR_COLLISION"))
        return "Compare the PART descriptor addresses and data ranges before any write.";
    if(!strcmp(code,"RDB_OUTSIDE_CONTAINER"))
        return "Check the parent MBR partition boundaries and RDB relative offsets in read-only mode.";
    if(!strcmp(code,"RDB_AUDIT_INVALID"))
        return "Check recorded geometry and metadata validity; current results cannot certify the layout.";
    return "Inspect this warning and the underlying source before changing any disk settings.";
}
static int notice(DollyInventoryNoticeEmit emit,void *user,
                  const char *code,const char *message,
                  DollyInventoryNoticeSeverity severity){
    DollyInventoryNotice n;
    n.code=code;n.message=message;n.severity=severity;
    n.suggested_action=suggestion_for(code);
    return emit(&n,user);
}
int dolly_inventory_partition_notices(const DollyPartition *p,
                                      DollyInventoryNoticeEmit emit,void *user){
    int result;
    DollyMaxTransferClass classification;
    if(!p||!emit)return -1;
    classification=dolly_maxtransfer_classify(p->max_transfer_bytes,
                                               p->max_transfer_present);
    switch(classification){
    case DOLLY_MT_NOT_PRESENT:
        result=notice(emit,user,"MAXTRANSFER_MISSING",
                      "MaxTransfer is not present in the PART record; active settings may differ.",
                      DOLLY_NOTICE_INFO);break;
    case DOLLY_MT_INVALID_ZERO:
        result=notice(emit,user,"MAXTRANSFER_ZERO",
                      "Zero MaxTransfer is suspicious; verify the active filesystem mount.",
                      DOLLY_NOTICE_WARNING);break;
    case DOLLY_MT_ABOVE_REFERENCE:
        result=notice(emit,user,"MAXTRANSFER_ABOVE_REFERENCE",
                      "Above legacy IDE reference 0x1FE00; check the controller before changing it.",
                      DOLLY_NOTICE_WARNING);break;
    case DOLLY_MT_BELOW_REFERENCE:
        result=notice(emit,user,"MAXTRANSFER_BELOW_REFERENCE",
                      "Below 0x1FE00; a stricter controller-specific limit may be intentional.",
                      DOLLY_NOTICE_INFO);break;
    case DOLLY_MT_AT_REFERENCE:
    default:result=0;break;
    }
    if(result)return result;
    if(p->sectors_per_block!=1){
        result=notice(emit,user,"SECTORS_PER_BLOCK_NONSTANDARD",
                      "de_SectorPerBlock differs from the documented value 1.",
                      DOLLY_NOTICE_WARNING);
        if(result)return result;
    }
    if(p->dma_mask_present && p->dma_mask==0){
        result=notice(emit,user,"MEMORY_MASK_ZERO",
                      "Memory address mask is zero; verify controller/active configuration.",
                      DOLLY_NOTICE_WARNING);
        if(result)return result;
    }
    if(!p->dos_type_present){
        result=notice(emit,user,"DOSTYPE_MISSING",
                      "DosType not present in this historical DosEnvec.",
                      DOLLY_NOTICE_INFO);
        if(result)return result;
    }
    return 0;
}
const char *dolly_notice_severity_name(DollyInventoryNoticeSeverity s){
    switch(s){
    case DOLLY_NOTICE_INFO:return "INFO";
    case DOLLY_NOTICE_WARNING:return "WARNING";
    case DOLLY_NOTICE_ERROR:return "ERROR";
    default:return "UNKNOWN";
    }
}

int dolly_inventory_rdb_notices(const DollyRdb *r,
                               uint64_t base,uint64_t length,
                               DollyInventoryNoticeEmit emit,void *user){
    DollyRdbAudit a;
    int x;
    if(!r||!emit)return -1;
    a=dolly_rdb_audit(r,base,length);
#define RDB_WARNING(flag,code,message) do { \
    if(a.flags&(flag)){ \
       x=notice(emit,user,code,message,DOLLY_NOTICE_WARNING); \
       if(x)return x; \
    } \
} while(0)
    RDB_WARNING(DOLLY_RDB_AUDIT_OVERLAPPING_PARTS,
                "RDB_PARTITIONS_OVERLAP",
                "Two recognized RDB partition data ranges overlap.");
    RDB_WARNING(DOLLY_RDB_AUDIT_HEADER_COLLISION,
                "RDB_HEADER_COLLISION",
                "Partition data overlaps the RDSK header block.");
    RDB_WARNING(DOLLY_RDB_AUDIT_PART_COLLISION,
                "RDB_PART_DESCRIPTOR_COLLISION",
                "Partition data overlaps a PART descriptor.");
    RDB_WARNING(DOLLY_RDB_AUDIT_OUTSIDE_CONTAINER,
                "RDB_OUTSIDE_CONTAINER",
                "A partition or RDB metadata block falls outside its container.");
    RDB_WARNING(DOLLY_RDB_AUDIT_BAD_ARGUMENT,
                "RDB_AUDIT_INVALID",
                "RDB geometry could not be safely audited.");
#undef RDB_WARNING
    return 0;
}
