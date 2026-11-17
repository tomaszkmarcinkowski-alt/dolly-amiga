/* Dolly native AmigaOS device backend — experimental read-only prototype.
 * Host tests use a mock Exec API, not a real AmigaOS compiler or driver.
 */
#include "dolly_amiga_source.h"
#include "dolly_io_plan.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef DOLLY_AMIGA_MOCK
#include "amiga_mock_exec.h"
#else
#include <exec/types.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/memory.h>
#include <proto/exec.h>
#endif
#ifndef MEMF_PUBLIC
#define MEMF_PUBLIC (1UL<<0)
#endif
#define DOLLY_NSCMD_DEVICEQUERY 0x4000u
#define DOLLY_NSD_TRACKDISK 5u
#define DOLLY_NSD_COMMAND_LIST_MAX 128u
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
    int opened;
    int supports_nsd64;
} DollyAmigaContext;
static void close_ctx(void *opaque) {
    DollyAmigaContext *ctx=(DollyAmigaContext *)opaque;
    if(!ctx)return;
    if(ctx->opened)CloseDevice((struct IORequest *)ctx->request);
    if(ctx->bounce)FreeMem(ctx->bounce,ctx->max_transfer);
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
    if(err || ctx->request->io_Error || actual<16 || actual>sizeof(q) ||
       q.SizeAvailable!=actual || q.DevQueryFormat!=0 ||
       q.DeviceType!=DOLLY_NSD_TRACKDISK || !q.SupportedCommands) return 0;
    cmd=q.SupportedCommands;
    for(i=0;i<DOLLY_NSD_COMMAND_LIST_MAX;i++){
        if(!cmd[i])break;
        if(cmd[i]==DOLLY_NSCMD_TD_READ64)return 1;
    }
    return 0;
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
                              ctx->supports_nsd64,&plan);
        if(status!=DOLLY_BLOCK_OK){r.status=status;return r;}
        ctx->request->io_Command=(UWORD)plan.command;
        ctx->request->io_Flags=0;
        ctx->request->io_Data=(APTR)ctx->bounce;
        ctx->request->io_Length=(ULONG)plan.byte_count;
        ctx->request->io_Offset=(ULONG)plan.offset_low;
        ctx->request->io_Actual=(ULONG)plan.offset_high;
        if(DoIO((struct IORequest *)ctx->request)!=0 || ctx->request->io_Error){
            r.status=DOLLY_BLOCK_EIO;return r;
        }
        if(ctx->request->io_Actual!=plan.byte_count){
            r.status=DOLLY_BLOCK_ESHORT;return r;
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
       transfer<sector||transfer>65536||transfer%sector)return DOLLY_BLOCK_EINVAL;
    ctx=(DollyAmigaContext *)calloc(1,sizeof(*ctx));
    if(!ctx)return DOLLY_BLOCK_ENOMEM;
    ctx->block_bytes=sector;ctx->max_transfer=transfer;
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
    ctx->bounce=(UBYTE *)AllocMem((ULONG)transfer,MEMF_PUBLIC);
    if(!ctx->bounce){close_ctx(ctx);return DOLLY_BLOCK_ENOMEM;}
    status=dolly_block_open_backend(opt->capacity_bytes,read_device,close_ctx,ctx,out);
    if(status!=DOLLY_BLOCK_OK)close_ctx(ctx);
    return status;
}
