#include "dolly_amiga_device_query.h"
#include "dolly_amiga_source.h"
#include "dolly_surface_log.h"
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <exec/devices.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>
#include <proto/exec.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Endpoint.path is constructed by our picker as "driver / Unit N".
 * The endpoint's byte-sized unit field is only an index, NOT the
 * actual controller unit number; never pass it to OpenDevice.
 */
const char *dolly_amiga_parse_unit(const char *path,char *driver,
                      unsigned capacity,ULONG *unit) {
    const char *marker,*digits,*p;
    ULONG number=0;
    size_t n;
    if(!path || !driver || !unit)return NULL;
    marker=strstr(path," / Unit ");
    if(!marker)return NULL;
    n=(size_t)(marker-path);
    if(!n || n>=capacity || n<7u)return NULL;
    /* Verify the driver suffix, no implicit device-name guesses. */
    if(strncmp(marker-7,".device",7)!=0)return NULL;
    digits=marker+8;
    if(*digits<'0' || *digits>'9')return NULL;
    p=digits;
    while(*p>='0' && *p<='9') {
        ULONG digit=(ULONG)(*p-'0');
        if(number>(0xfffffffful-digit)/10ul)return NULL;
        number=number*10ul+digit;
        ++p;
    }
    if(*p!='\0' && strncmp(p," / ",3)!=0)return NULL;
    memcpy(driver,path,n);
    driver[n]='\0';
    *unit=number;
    return p;
}

/* Reject corrupt/nonstandard geometry instead of enabling selection
 * based on an arbitrary nonzero size. Both probes use this policy.
 */
static int valid_sector_bytes(ULONG n) {
    return n>=128ul && n<=65536ul && (n&(n-1ul))==0;
}
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
static int inquiry(struct IOExtTD *io,UBYTE *data) {
    struct SCSICmd sc={0};
    memset(data,0,64);
    data[40]=0x12u;
    data[44]=36u;
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
    return DoIO((struct IORequest *)io)==0 && sc.scsi_Status==0 && sc.scsi_Actual>=36;
}
static int inquiry_identity_valid(const UBYTE *data) {
    unsigned i;
    int visible=0;
    for(i=8;i<32u;++i) {
        UBYTE ch=data[i];
        if(ch && (ch<32u || ch>126u))return 0;
        if(ch>32u && ch!='?')visible=1;
    }
    return visible;
}
static int read_legacy_ide_sector(struct IOExtTD *io) {
    UBYTE *block=AllocMem(512,MEMF_CHIP);
    int ok;
    if(!block)return 0;
    io->iotd_Req.io_Command=CMD_READ;
    io->iotd_Req.io_Data=block;
    io->iotd_Req.io_Length=512;
    io->iotd_Req.io_Offset=0;
    io->iotd_Req.io_Actual=0;
    io->iotd_Req.io_Flags=0;
    ok=DoIO((struct IORequest *)io)==0 && io->iotd_Req.io_Actual==512;
    FreeMem(block,512);
    return ok;
}

/* MODE SENSE(6), current values. The DMA buffer also holds the CDB. */
static const UBYTE *mode_page(struct IOExtTD *io,UBYTE *data,
                              UBYTE page,unsigned minimum) {
    struct SCSICmd sc;
    unsigned length,offset;
    memset(data,0,64);
    memset(&sc,0,sizeof(sc));
    data[40]=0x1au;
    data[41]=0x08u; /* Disable block descriptors. */
    data[42]=page;
    data[44]=40u;
    sc.scsi_Data=(UWORD *)data;
    sc.scsi_Length=40;
    sc.scsi_Command=data+40;
    sc.scsi_CmdLength=6;
    sc.scsi_Flags=SCSIF_READ;
    io->iotd_Req.io_Command=HD_SCSICMD;
    io->iotd_Req.io_Data=&sc;
    io->iotd_Req.io_Length=sizeof(sc);
    io->iotd_Req.io_Offset=0;
    io->iotd_Req.io_Flags=0;
    if(DoIO((struct IORequest *)io)!=0 || sc.scsi_Status!=0 ||
       sc.scsi_Actual<4 || sc.scsi_Actual>40)return NULL;
    length=(unsigned)data[0]+1u;
    if(length>sc.scsi_Actual)length=(unsigned)sc.scsi_Actual;
    offset=4u+data[3];
    if(offset+minimum>length || (data[offset]&0x7fu)!=page ||
       (unsigned)data[offset+1]+2u<minimum ||
       offset+(unsigned)data[offset+1]+2u>length)return NULL;
    return data+offset;
}
static UWORD ata_word(const UBYTE *data,unsigned word) {
    return (UWORD)(data[2u*word] | ((UWORD)data[2u*word+1u]<<8));
}
static void query_ata_identify(struct IOExtTD *io,char *out,unsigned capacity) {
    struct SCSICmd sc={0};
    UBYTE *data=AllocMem(528,MEMF_CHIP|MEMF_CLEAR);
    UBYTE *cdb;
    UWORD cylinders,heads,sectors;
    uint64_t lba=0;
    char count[21];
    unsigned i;
    if(!data)return;
    cdb=data+512;
    cdb[0]=0x85; /* SAT ATA PASS-THROUGH(16) */
    cdb[1]=4u<<1; /* PIO data-in */
    cdb[2]=0x0e; /* device to host, length in blocks, sector count */
    cdb[6]=1;
    cdb[14]=0xec; /* IDENTIFY DEVICE */
    sc.scsi_Data=(UWORD *)data;
    sc.scsi_Length=512;
    sc.scsi_Command=cdb;
    sc.scsi_CmdLength=16;
    sc.scsi_Flags=SCSIF_READ;
    io->iotd_Req.io_Command=HD_SCSICMD;
    io->iotd_Req.io_Data=&sc;
    io->iotd_Req.io_Length=sizeof(sc);
    io->iotd_Req.io_Offset=0;
    io->iotd_Req.io_Flags=0;
    if(DoIO((struct IORequest *)io)!=0 || sc.scsi_Status!=0 ||
       sc.scsi_Actual!=512 || !ata_word(data,0) ||
       (ata_word(data,0)&0x8000u))goto done;
    cylinders=ata_word(data,1);heads=ata_word(data,3);sectors=ata_word(data,6);
    if((ata_word(data,53)&1u) && ata_word(data,54) &&
       ata_word(data,55) && ata_word(data,56)) {
        cylinders=ata_word(data,54);heads=ata_word(data,55);sectors=ata_word(data,56);
    }
    if((ata_word(data,83)&0xc400u)==0x4400u)
        for(i=0;i<4u;++i)lba|=(uint64_t)ata_word(data,100u+i)<<(16u*i);
    if(!lba && (ata_word(data,49)&0x0200u))
        lba=(uint32_t)ata_word(data,60)|((uint32_t)ata_word(data,61)<<16);
    if(lba>0x0000ffffffffffffULL)lba=0;
    if(lba)dolly_surface_log_decimal(count,lba);
    else strcpy(count,"Unavailable");
    if(cylinders && heads && sectors)
        snprintf(out,capacity,"CHS %u/%u/%u | LBA sectors %s",
                 (unsigned)cylinders,(unsigned)heads,(unsigned)sectors,count);
    else snprintf(out,capacity,"CHS unavailable | LBA sectors %s",count);
done:
    FreeMem(data,528);
}
int dolly_amiga_device_query(const char *driver,ULONG unit,
                            char *name,unsigned name_capacity,
                            char *size,unsigned size_capacity,
                            DollyMediaGeometry *geometry,int probe_chs,
                            char *ata,unsigned ata_capacity) {
    struct MsgPort *port=NULL;
    struct IOExtTD *io=NULL;
    struct DriveGeometry geom;
    struct SCSICmd sc;
    UBYTE *data=NULL;
    LONG rc;
    int opened=0,have_capacity=0,have_name=0,have_td_geometry=0;
    if(ata && ata_capacity)assign_string(ata,ata_capacity,"Unavailable");
    if(!name || !size || !name_capacity || !size_capacity ||
       !driver || !*driver)return 0;
    if(geometry)memset(geometry,0,sizeof(*geometry));
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
    if(rc==0 && geom.dg_TotalSectors &&
       valid_sector_bytes(geom.dg_SectorSize)) {
        capacity_string(size,size_capacity,
            (uint64_t)geom.dg_TotalSectors*
            (uint64_t)geom.dg_SectorSize);
        have_capacity=1;
        have_td_geometry=1;
        if(geometry) {
            geometry->block_count=geom.dg_TotalSectors;
            geometry->block_bytes=geom.dg_SectorSize;
            geometry->capacity_valid=1;
            geometry->capacity_source=DOLLY_GEOMETRY_TRACKDISK;
            geometry->cylinders=geom.dg_Cylinders;
            geometry->heads=geom.dg_Heads;
            geometry->sectors_per_track=geom.dg_TrackSectors;
        }
    }
    data=(UBYTE *)AllocMem(64,MEMF_CHIP|MEMF_CLEAR);
    if(data) {
        char vendor[9],product[17],combined[32];
        int answered=inquiry(io,data);
        if(answered && !inquiry_identity_valid(data) &&
           (!have_td_geometry || geom.dg_SectorSize==512u) &&
           io->iotd_Req.io_Device &&
           io->iotd_Req.io_Device->dd_Library.lib_Version==37u &&
           !strcmp(driver,"scsi.device") && dolly_amiga_probe_rom_scsi(unit)) {
            /* FS-UAE 3.1.66 may retain its IDE buffer offset after a
             * multi-block transfer. READ resets it before IDENTIFY. */
            if(!read_legacy_ide_sector(io) || !inquiry(io,data) ||
               !inquiry_identity_valid(data))goto cleanup;
        }
        if(answered) {
            ascii_trim(vendor,sizeof(vendor),data+8,8);
            ascii_trim(product,sizeof(product),data+16,16);
            if(vendor[0] && product[0]) {
                sprintf(combined,"%s %s",vendor,product);
                assign_string(name,name_capacity,combined);
                have_name=1;
            } else if(product[0]) {
                assign_string(name,name_capacity,product);have_name=1;
            } else if(vendor[0]) {
                assign_string(name,name_capacity,vendor);have_name=1;
            }
        }
        {
            ULONG last,block;
            /* TD_GETGEOMETRY can report a translated, cylinder-rounded size.
             * READ CAPACITY reports the device's last addressable LBA. */
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
                if(last!=0xfffffffful && valid_sector_bytes(block) &&
                   (!have_td_geometry || block==geom.dg_SectorSize)) {
                    capacity_string(size,size_capacity,
                       ((uint64_t)last+1u)*(uint64_t)block);
                    have_capacity=1;
                    if(geometry) {
                        geometry->block_count=(uint64_t)last+1u;
                        geometry->block_bytes=block;
                        geometry->capacity_valid=1;
                        geometry->capacity_source=DOLLY_GEOMETRY_SCSI_CAPACITY10;
                    }
                }
            }
        }
        if(probe_chs && geometry && have_capacity && !have_td_geometry) {
            const UBYTE *page=mode_page(io,data,4u,6u);
            if(page) {
                geometry->cylinders=((ULONG)page[2]<<16)|
                                    ((ULONG)page[3]<<8)|page[4];
                geometry->heads=page[5];
            }
            page=mode_page(io,data,3u,12u);
            if(page)
                geometry->sectors_per_track=((ULONG)page[10]<<8)|page[11];
        }
    }
    if(ata && ata_capacity && have_capacity)query_ata_identify(io,ata,ata_capacity);
    /* A successful INQUIRY alone never verifies physical media. */
    if(have_capacity && !have_name && geometry && !probe_chs) {
        char info[56];
        sprintf(info,"%lu B/sector (%s)",
                (unsigned long)geometry->block_bytes,
                geometry->capacity_source==DOLLY_GEOMETRY_TRACKDISK ?
                "TD geometry" : "SCSI capacity");
        assign_string(name,name_capacity,info);
    }
cleanup:
    if(opened)CloseDevice((struct IORequest *)io);
    if(data)FreeMem(data,64);
    if(io)DeleteIORequest((struct IORequest *)io);
    if(port)DeleteMsgPort(port);
    return opened && (have_name || have_capacity);
}
