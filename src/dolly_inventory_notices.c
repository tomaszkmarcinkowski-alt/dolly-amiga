#include "dolly_inventory_notices.h"
#include "dolly_maxtransfer.h"
static int notice(DollyInventoryNoticeEmit emit,void *user,
                  const char *code,const char *message,
                  DollyInventoryNoticeSeverity severity){
    DollyInventoryNotice n;
    n.code=code;n.message=message;n.severity=severity;
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
