/* Dolly native AmigaOS device backend — experimental read-only prototype.
 * Host tests use a mock Exec API, not a real AmigaOS compiler or driver.
 */
#include "dolly_amiga_source.h"
#include "dolly_io_plan.h"
#include "dolly_dma_policy.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef DOLLY_AMIGA_MOCK
#include "amiga_mock_exec.h"
#else
#include <exec/types.h>
#include <exec/errors.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/memory.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>
#include <proto/exec.h>
#endif
#ifndef MEMF_PUBLIC
#define MEMF_PUBLIC (1UL<<0)
#endif
#ifndef MEMF_24BITDMA
#define MEMF_24BITDMA (1UL<<9)
#endif
#define DOLLY_NSCMD_DEVICEQUERY 0x4000u
#define DOLLY_NSD_TRACKDISK 5u
#define DOLLY_NSD_COMMAND_LIST_MAX 128u
#define LEGACY_READ_END UINT64_C(0x100000000)
#define UAE_IDE_SECTOR_LIMIT UINT64_C(0x10000000)
#define SCSI_CMD_BYTES 16u
#define SCSI_SENSE_BYTES 32u
struct DollyNsdQuery {
    ULONG DevQueryFormat;
    ULONG SizeAvailable;
    UWORD DeviceType;
    UWORD DeviceSubType;
    UWORD *SupportedCommands;
};
typedef struct DollyAmigaContext {
    struct MsgPort *port;
    struct IOStdReq *request;
    UBYTE *bounce;
    ULONG block_bytes;
    ULONG max_transfer;
    ULONG buffer_bytes;
    int opened;
    int supports_nsd64;
    int scsi10;
    ULONG dma_mask;
    ULONG dma_alignment;
} DollyAmigaContext;
static void notify_dma(const DollyAmigaDeviceOptions *opt,DollyDmaStatus reason){
    if(opt && opt->report_dma_problem)
        opt->report_dma_problem(reason,dolly_dma_user_message(reason),opt->report_user);
}
static void close_ctx(void *opaque) {
    DollyAmigaContext *ctx=(DollyAmigaContext *)opaque;
    if(!ctx)return;
    if(ctx->opened)CloseDevice((struct IORequest *)ctx->request);
    if(ctx->bounce)FreeMem(ctx->bounce,ctx->buffer_bytes);
    if(ctx->request)DeleteIORequest((struct IORequest *)ctx->request);
    if(ctx->port)DeleteMsgPort(ctx->port);
    free(ctx);
}
static int verify_nsd64(DollyAmigaContext *ctx){
    struct DollyNsdQuery q;
    ULONG actual;
    ULONG i;
    LONG err;
    UWORD *cmd;
    memset(&q,0,sizeof(q));
    ctx->request->io_Command=DOLLY_NSCMD_DEVICEQUERY;
    ctx->request->io_Flags=0;
    ctx->request->io_Offset=0;
    ctx->request->io_Actual=0;
    ctx->request->io_Length=sizeof(q);
    ctx->request->io_Data=(APTR)&q;
    err=DoIO((struct IORequest *)ctx->request);
    actual=ctx->request->io_Actual;
    if(err || ctx->request->io_Error || actual!=sizeof(q) ||
       q.SizeAvailable!=actual || q.DevQueryFormat!=0 ||
       q.DeviceType!=DOLLY_NSD_TRACKDISK || !q.SupportedCommands) return 0;
    cmd=q.SupportedCommands;
    for(i=0;i<DOLLY_NSD_COMMAND_LIST_MAX;i++){
        if(!cmd[i])break;
        if(cmd[i]==DOLLY_NSCMD_TD_READ64)return 1;
    }
    return 0;
}
static DollyBlockStatus request_error(const struct IOStdReq *request) {
#ifndef DOLLY_AMIGA_MOCK
    switch(request->io_Error) {
    case IOERR_NOCMD: return DOLLY_BLOCK_EUNSUPPORTED;
    case IOERR_BADLENGTH:
    case IOERR_BADADDRESS: return DOLLY_BLOCK_EINVAL;
    case IOERR_OPENFAIL:
    case IOERR_ABORTED:
    case IOERR_UNITBUSY:
    case TDERR_DiskChanged: return DOLLY_BLOCK_EDEVICE;
    }
#else
    (void)request;
#endif
    return DOLLY_BLOCK_EIO;
}
static DollyBlockStatus read_scsi10(DollyAmigaContext *ctx,uint64_t off,
                                   ULONG length) {
    struct SCSICmd sc;
    struct IOStdReq *request=ctx->request;
    UBYTE *cdb=ctx->bounce+ctx->max_transfer;
    UBYTE *sense=cdb+SCSI_CMD_BYTES;
    uint64_t lba=off/ctx->block_bytes;
    ULONG blocks=length/ctx->block_bytes;
    LONG result;
    if(lba+blocks>UAE_IDE_SECTOR_LIMIT)return DOLLY_BLOCK_EUNSUPPORTED;
    memset(&sc,0,sizeof(sc));
    memset(cdb,0,SCSI_CMD_BYTES+SCSI_SENSE_BYTES);
    cdb[0]=0x28u;
    cdb[2]=(UBYTE)(lba>>24);
    cdb[3]=(UBYTE)(lba>>16);
    cdb[4]=(UBYTE)(lba>>8);
    cdb[5]=(UBYTE)lba;
    cdb[7]=(UBYTE)(blocks>>8);
    cdb[8]=(UBYTE)blocks;
    sc.scsi_Data=(UWORD *)ctx->bounce;
    sc.scsi_Length=length;
    sc.scsi_Command=cdb;
    sc.scsi_CmdLength=10;
    sc.scsi_Flags=SCSIF_READ|SCSIF_AUTOSENSE;
    sc.scsi_SenseData=sense;
    sc.scsi_SenseLength=SCSI_SENSE_BYTES;
    request->io_Command=HD_SCSICMD;
    request->io_Flags=0;
    request->io_Error=0;
    request->io_Offset=0;
    request->io_Actual=0;
    request->io_Data=&sc;
    request->io_Length=sizeof(sc);
    result=DoIO((struct IORequest *)request);
    if(sc.scsi_Status) {
        unsigned response=sense[0]&0x7fu;
        unsigned key=0;
        if(sc.scsi_SenseActual>=3 && (response==0x70u || response==0x71u))
            key=sense[2]&15u;
        if(sc.scsi_SenseActual>=2 && (response==0x72u || response==0x73u))
            key=sense[1]&15u;
        if(key==5u)return DOLLY_BLOCK_EUNSUPPORTED;
        if(key==3u || key==4u)return DOLLY_BLOCK_EIO;
        return DOLLY_BLOCK_EDEVICE;
    }
    if(result || request->io_Error)return request_error(request);
    return sc.scsi_Actual==length?DOLLY_BLOCK_OK:DOLLY_BLOCK_ESHORT;
}
static DollyBlockReadResult read_device(void *opaque,uint64_t off,
                                       void *dst,size_t length){
    DollyAmigaContext *ctx=(DollyAmigaContext *)opaque;
    DollyBlockReadResult r={DOLLY_BLOCK_OK,0};
    UBYTE *bytes=(UBYTE *)dst;
    while(length){
        DollyIoReadPlan plan;
        DollyBlockStatus status=dolly_io_plan_read(off,length,
                              (uint32_t)ctx->block_bytes,
                              (uint32_t)ctx->max_transfer,
                              ctx->supports_nsd64 || ctx->scsi10,&plan);
        if(status!=DOLLY_BLOCK_OK){r.status=status;return r;}
        if(ctx->scsi10 && plan.command==DOLLY_NSCMD_TD_READ64) {
            status=read_scsi10(ctx,off,plan.byte_count);
            if(status!=DOLLY_BLOCK_OK){r.status=status;return r;}
        } else {
            ctx->request->io_Command=(UWORD)plan.command;
            ctx->request->io_Flags=0;
            ctx->request->io_Error=0;
            ctx->request->io_Data=(APTR)ctx->bounce;
            ctx->request->io_Length=(ULONG)plan.byte_count;
            ctx->request->io_Offset=(ULONG)plan.offset_low;
            ctx->request->io_Actual=(ULONG)plan.offset_high;
            if(DoIO((struct IORequest *)ctx->request)!=0 || ctx->request->io_Error){
                r.status=request_error(ctx->request);return r;
            }
            if(ctx->request->io_Actual!=plan.byte_count){
                r.status=DOLLY_BLOCK_ESHORT;return r;
            }
        }
        memcpy(bytes,ctx->bounce,plan.byte_count);
        bytes+=plan.byte_count;off+=plan.byte_count;
        length-=plan.byte_count;r.bytes_read+=plan.byte_count;
    }
    return r;
}
DollyBlockStatus dolly_amiga_open_device(const DollyAmigaDeviceOptions *opt,
                                        DollyBlockSource **out){
    DollyBlockStatus status;
    DollyAmigaContext *ctx;
    uint32_t sector,transfer;
    if(!out||!opt||!opt->device_name||!*opt->device_name)return DOLLY_BLOCK_EINVAL;
    *out=NULL;
    sector=opt->physical_block_bytes;
    transfer=opt->max_transfer_bytes;
    if(sector<512||sector>32768||(sector&(sector-1))||
       !opt->capacity_bytes || opt->capacity_bytes%sector ||
       transfer<sector||transfer>65536||transfer%sector ||
       opt->required_read_end>opt->capacity_bytes ||
       opt->required_read_end%sector)return DOLLY_BLOCK_EINVAL;
    /* A caller must establish device/controller DMA limitations before
     * raw access. It is not safe to guess from a generic device name. */
    if(opt->dma_profile_verified!=1 ||
       opt->dma_address_mask==0 ||
       opt->dma_alignment<2 || opt->dma_alignment>4096 ||
       (opt->dma_alignment&(opt->dma_alignment-1))!=0 ||
       opt->require_24bit_dma>1 || !opt->report_dma_problem){
        notify_dma(opt,DOLLY_DMA_BAD_PROFILE);
        return DOLLY_BLOCK_EINVAL;
    }
    ctx=(DollyAmigaContext *)calloc(1,sizeof(*ctx));
    if(!ctx)return DOLLY_BLOCK_ENOMEM;
    ctx->block_bytes=sector;ctx->max_transfer=transfer;
    ctx->dma_mask=opt->dma_address_mask;
    ctx->dma_alignment=opt->dma_alignment;
    ctx->port=CreateMsgPort();
    if(!ctx->port){close_ctx(ctx);return DOLLY_BLOCK_ENOMEM;}
    /* NSD recommends extra packet room for drivers with larger requests. */
    ctx->request=(struct IOStdReq *)CreateIORequest(ctx->port,sizeof(struct IOStdReq)+128);
    if(!ctx->request){close_ctx(ctx);return DOLLY_BLOCK_ENOMEM;}
    if(OpenDevice((CONST_STRPTR)opt->device_name,(ULONG)opt->unit,
                  (struct IORequest *)ctx->request,0)!=0){
        close_ctx(ctx);return DOLLY_BLOCK_EIO;
    }
    ctx->opened=1;
    ctx->supports_nsd64=verify_nsd64(ctx);
    if(opt->required_read_end>LEGACY_READ_END && !ctx->supports_nsd64) {
        if(opt->verified_uae_ide!=1 ||
           opt->required_read_end/sector>UAE_IDE_SECTOR_LIMIT) {
            close_ctx(ctx);
            return DOLLY_BLOCK_EUNSUPPORTED;
        }
        ctx->scsi10=1;
    }
    ctx->buffer_bytes=transfer+(ctx->scsi10?SCSI_CMD_BYTES+SCSI_SENSE_BYTES:0u);
    ctx->bounce=(UBYTE *)AllocMem(ctx->buffer_bytes,
                  MEMF_PUBLIC | (opt->require_24bit_dma?MEMF_24BITDMA:0));
    if(!ctx->bounce){
        notify_dma(opt,DOLLY_DMA_NO_COMPATIBLE_MEMORY);
        close_ctx(ctx);return DOLLY_BLOCK_ENOMEM;
    }
    /* This is a necessary address-range check, NOT sufficient proof of
     * bus visibility or cache coherency. Integration still needs real
     * controller testing and OS-specific DMA handling. */
#ifdef DOLLY_AMIGA_MOCK
    {
        uint64_t addr=dolly_mock_dma_address();
#else
    {
        uint64_t addr=(uint64_t)(uintptr_t)ctx->bounce;
#endif
        DollyDmaStatus dma_result=dolly_dma_check_buffer(addr,ctx->buffer_bytes,
                                            ctx->dma_mask,ctx->dma_alignment);
        if(dma_result!=DOLLY_DMA_OK){
            notify_dma(opt,dma_result);
            close_ctx(ctx);return DOLLY_BLOCK_EUNSUPPORTED;
        }
    }
    status=dolly_block_open_backend(opt->capacity_bytes,read_device,close_ctx,ctx,out);
    if(status!=DOLLY_BLOCK_OK)close_ctx(ctx);
    return status;
}
