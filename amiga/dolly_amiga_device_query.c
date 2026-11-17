#include "dolly_amiga_device_query.h"
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>
#include <proto/exec.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static ULONG read_be32(const UBYTE *p) {
    return ((ULONG)p[0]<<24)|((ULONG)p[1]<<16)|
           ((ULONG)p[2]<<8)|(ULONG)p[3];
}
static void ascii_trim(char *dst,unsigned cap,const UBYTE *src,
                       unsigned len) {
    unsigned i;
    if(!cap)return;
    if(len>=cap)len=cap-1u;
    for(i=0;i<len;++i) {
        UBYTE ch=src[i];
        dst[i]=(ch>=32u && ch<=126u)?(char)ch:'?';
    }
    while(len && dst[len-1u]==' ')--len;
    dst[len]='\0';
}
static void assign_string(char *to,unsigned cap,const char *from) {
    unsigned i=0;
    if(!cap)return;
    while(i+1u<cap && from[i]) {
        to[i]=from[i];
        ++i;
    }
    to[i]='\0';
}
static void capacity_string(char *out,unsigned cap,uint64_t bytes) {
    char buffer[40];
    /* 64-bit quotient is bounded by the capacity of READ CAPACITY(10)
     * on this classic API, so it fits unsigned long in MiB.
     */
    sprintf(buffer,"%lu MiB",(unsigned long)(bytes/1048576u));
    assign_string(out,cap,buffer);
}
int dolly_amiga_device_query_ex(const char *driver,ULONG unit,
                               char *name,unsigned name_capacity,
                               char *size,unsigned size_capacity,
                               DollyDeviceQueryInfo *info) {
    struct MsgPort *port=NULL;
    struct IOExtTD *io=NULL;
    struct DriveGeometry geom;
    struct SCSICmd sc;
    UBYTE *data=NULL;
    LONG rc;
    int opened=0,have_capacity=0;
    if(info)memset(info,0,sizeof(*info));
    if(!name || !size || !name_capacity || !size_capacity ||
       !driver || !*driver)return 0;
    assign_string(name,name_capacity,"Unavailable");
    assign_string(size,size_capacity,"Unavailable");
    /* The generic disk picker never opens floppy tracks. carddisk
     * remains a candidate but might reject either query.
     */
    if(strcmp(driver,"trackdisk.device")==0)return 0;
    port=CreateMsgPort();
    if(!port)return 0;
    io=(struct IOExtTD *)CreateIORequest(port,sizeof(*io));
    if(!io)goto cleanup;
    rc=OpenDevice((STRPTR)driver,unit,(struct IORequest *)io,0);
    if(rc)goto cleanup;
    opened=1;
    memset(&geom,0,sizeof(geom));
    io->iotd_Req.io_Command=TD_GETGEOMETRY;
    io->iotd_Req.io_Data=&geom;
    io->iotd_Req.io_Length=sizeof(geom);
    io->iotd_Req.io_Offset=0;
    io->iotd_Req.io_Flags=0;
    rc=DoIO((struct IORequest *)io);
    if(rc==0 && geom.dg_TotalSectors && geom.dg_SectorSize) {
        capacity_string(size,size_capacity,
            (uint64_t)geom.dg_TotalSectors*
            (uint64_t)geom.dg_SectorSize);
        have_capacity=1;
        if(info) {
            info->capacity_bytes=
                 (uint64_t)geom.dg_TotalSectors*geom.dg_SectorSize;
            info->sector_bytes=geom.dg_SectorSize;
            info->capacity_valid=1;
        }
    }
    data=(UBYTE *)AllocMem(64,MEMF_CHIP|MEMF_CLEAR);
    if(data) {
        char vendor[9],product[17],combined[32];
        memset(&sc,0,sizeof(sc));
        data[40]=0x12u;data[44]=36u;
        sc.scsi_Data=(UWORD *)data;
        sc.scsi_Length=36;
        sc.scsi_Command=data+40;
        sc.scsi_CmdLength=6;
        sc.scsi_Flags=SCSIF_READ;
        io->iotd_Req.io_Command=HD_SCSICMD;
        io->iotd_Req.io_Data=&sc;
        io->iotd_Req.io_Length=sizeof(sc);
        io->iotd_Req.io_Offset=0;
        io->iotd_Req.io_Flags=0;
        rc=DoIO((struct IORequest *)io);
        if(rc==0 && sc.scsi_Status==0 && sc.scsi_Actual>=36) {
            if(info)info->identity_valid=1;
            ascii_trim(vendor,sizeof(vendor),data+8,8);
            ascii_trim(product,sizeof(product),data+16,16);
            if(vendor[0] && product[0]) {
                sprintf(combined,"%s %s",vendor,product);
                assign_string(name,name_capacity,combined);
            } else if(product[0])assign_string(name,name_capacity,product);
            else if(vendor[0])assign_string(name,name_capacity,vendor);
        }
        if(!have_capacity) {
            ULONG last,block;
            memset(data,0,64);
            memset(&sc,0,sizeof(sc));
            data[40]=0x25u;  /* READ CAPACITY (10) */
            sc.scsi_Data=(UWORD *)data;
            sc.scsi_Length=8;
            sc.scsi_Command=data+40;
            sc.scsi_CmdLength=10;
            sc.scsi_Flags=SCSIF_READ;
            io->iotd_Req.io_Command=HD_SCSICMD;
            io->iotd_Req.io_Data=&sc;
            io->iotd_Req.io_Length=sizeof(sc);
            io->iotd_Req.io_Offset=0;
            io->iotd_Req.io_Flags=0;
            rc=DoIO((struct IORequest *)io);
            if(rc==0 && sc.scsi_Status==0 && sc.scsi_Actual>=8) {
                last=read_be32(data);
                block=read_be32(data+4);
                if(last!=0xfffffffful &&
                   block>=128ul && block<=65536ul) {
                    uint64_t bytes=((uint64_t)last+1u)*(uint64_t)block;
                    capacity_string(size,size_capacity,bytes);
                    have_capacity=1;
                    if(info) {
                        info->capacity_bytes=bytes;
                        info->sector_bytes=block;
                        info->capacity_valid=1;
                    }
                }
            }
        }
    }
cleanup:
    if(opened)CloseDevice((struct IORequest *)io);
    if(data)FreeMem(data,64);
    if(io)DeleteIORequest((struct IORequest *)io);
    if(port)DeleteMsgPort(port);
    return opened;
}

int dolly_amiga_device_query(const char *driver,ULONG unit,
                            char *name,unsigned name_capacity,
                            char *size,unsigned size_capacity) {
    return dolly_amiga_device_query_ex(driver,unit,name,name_capacity,
                                      size,size_capacity,NULL);
}
