#include "dolly_amiga_partition_scan.h"
#include "dolly_amiga_physical_catalog.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_source.h"
#include "dolly_amiga_write.h"
#include "dolly_surface_log.h"
#include <exec/devices.h>
#include <stdlib.h>
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/trackdisk.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define BLK 512u
#define RDB_END 0xfffffffful

static ULONG be32(const UBYTE *p) {
    return ((ULONG)p[0]<<24)|((ULONG)p[1]<<16)|
           ((ULONG)p[2]<<8)|(ULONG)p[3];
}
static ULONG le32(const UBYTE *p) {
    return (ULONG)p[0]|((ULONG)p[1]<<8)|
           ((ULONG)p[2]<<16)|((ULONG)p[3]<<24);
}
static int mul_checked(uint64_t a,uint64_t b,uint64_t *out) {
    if(b && a>UINT64_MAX/b)return 0;
    *out=a*b;
    return 1;
}
static int add_checked(uint64_t a,uint64_t b,uint64_t *out) {
    if(a>UINT64_MAX-b)return 0;
    *out=a+b;
    return 1;
}
typedef struct Reader {
    struct IOExtTD *io;
    UBYTE *buffer;
    uint64_t total_bytes;
    ULONG base_lba;
    int unreadable;
    const UBYTE *snapshot;
    ULONG snapshot_bytes;
} Reader;
static int load_sector(Reader *r,ULONG relative_lba) {
    uint64_t lba=(uint64_t)r->base_lba+(uint64_t)relative_lba;
    uint64_t pos=lba*BLK;
    struct IOStdReq *io;
    if(r->snapshot) {
        if(pos>r->snapshot_bytes || BLK>r->snapshot_bytes-pos)return 0;
        memcpy(r->buffer,r->snapshot+(size_t)pos,BLK);
        return 1;
    }
    io=&r->io->iotd_Req;
    if(pos>r->total_bytes || BLK>r->total_bytes-pos ||
       pos>0xfffffffful)return 0;
    io->io_Command=CMD_READ;
    io->io_Data=r->buffer;
    io->io_Length=BLK;
    io->io_Offset=(ULONG)pos;
    io->io_Flags=0;
    io->io_Error=0;
    io->io_Actual=0;
    if(DoIO((struct IORequest *)r->io)!=0 ||
       io->io_Error || io->io_Actual!=BLK) {
        r->unreadable=1;
        return 0;
    }
    return 1;
}
static int checksum(const UBYTE *buf,ULONG minimum) {
    ULONG words=be32(buf+4),sum=0,i;
    if(words<minimum || words>BLK/4u)return 0;
    for(i=0;i<words;++i)sum+=be32(buf+4u*i);
    return sum==0;
}
static void safe_label(char *dest,const UBYTE *src,unsigned len) {
    unsigned i;
    if(len>31u)len=31u;
    for(i=0;i<len;++i) {
        UBYTE ch=src[i];
        dest[i]=(ch>=33u && ch<=126u)?(char)ch:'_';
    }
    dest[len]='\0';
}
static void dos_name(char *dest,ULONG dos) {
    unsigned i;
    for(i=0;i<3u;++i) {
        UBYTE ch=(UBYTE)(dos>>(24u-8u*i));
        dest[i]=(ch>=33u && ch<=126u)?(char)ch:'?';
    }
    dest[3]=(char)((dos&0xfful)<10u?'0'+(dos&0xfful):'?');
    dest[4]='\0';
}
static int scan_rdb(Reader *r,DollyNativePartitionMap *out,
                    ULONG base_lba,ULONG scan_limit) {
    ULONG block,head=RDB_END,header_block=RDB_END;
    ULONG seen[DOLLY_PART_MAX];
    ULONG reserved_lo=0,reserved_hi=0;
    int backup_range_ok=0;
    unsigned used=0,i,j;
    r->base_lba=base_lba;
    for(block=0;block<scan_limit;++block) {
        if(!load_sector(r,block)) {
            strcpy(out->status,"RDB read failed");
            return -1;
        }
        if(memcmp(r->buffer,"RDSK",4)!=0)continue;
        if(!checksum(r->buffer,19u) || be32(r->buffer+16)!=BLK) {
            strcpy(out->status,"Invalid RDB header or checksum");
            return -1;
        }
        head=be32(r->buffer+28);
        header_block=block;
        if(base_lba==0 && be32(r->buffer+4)>=39u) {
            ULONG offset;
            reserved_lo=be32(r->buffer+128);
            reserved_hi=be32(r->buffer+132);
            backup_range_ok=reserved_lo<=block && block<=reserved_hi &&
                reserved_hi<=0xfffffffful/BLK &&
                ((uint64_t)reserved_hi+1u)*BLK<=r->total_bytes &&
                be32(r->buffer+152)<=reserved_hi;
            for(offset=24u;offset<=36u;offset+=4u) {
                ULONG link=be32(r->buffer+offset);
                if(link!=RDB_END && (link<reserved_lo || link>reserved_hi))
                    backup_range_ok=0;
            }
        }
        break;
    }
    if(header_block==RDB_END)return 0;
    while(head!=RDB_END) {
        ULONG next,envwords,blockwords,heads,spt,low,high,dos;
        uint64_t cyl_bytes,begin,length,end;
        DollyNativePartition *p;
        char type[5],label_copy[40];
        if(used>=DOLLY_PART_MAX) {
            strcpy(out->status,"Too many RDB partitions");
            return -1;
        }
        for(j=0;j<used;++j)if(seen[j]==head) {
            strcpy(out->status,"Cycle in RDB PART chain");
            return -1;
        }
        seen[used++]=head;
        if(head<reserved_lo || head>reserved_hi)backup_range_ok=0;
        if(!load_sector(r,head) || memcmp(r->buffer,"PART",4)!=0) {
            strcpy(out->status,"Invalid RDB PART block");
            return -1;
        }
        envwords=be32(r->buffer+128);
        if(envwords<11u || envwords>95u ||
           !checksum(r->buffer,33u+envwords)) {
            strcpy(out->status,"Invalid RDB PART checksum");
            return -1;
        }
        next=be32(r->buffer+16);
        blockwords=be32(r->buffer+132);
        heads=be32(r->buffer+140);
        spt=be32(r->buffer+148);
        low=be32(r->buffer+164);
        high=be32(r->buffer+168);
        dos=envwords>=16u?be32(r->buffer+192):0ul;
        if(!heads || !spt || blockwords<BLK/4u ||
           blockwords%(BLK/4u)!=0 || blockwords>8192ul || high<low) {
            strcpy(out->status,"Invalid RDB geometry");
            return -1;
        }
        if(!mul_checked(heads,spt,&cyl_bytes) ||
           !mul_checked(cyl_bytes,blockwords,&cyl_bytes) ||
           !mul_checked(cyl_bytes,4u,&cyl_bytes) ||
           !mul_checked(low,cyl_bytes,&begin) ||
           !add_checked((uint64_t)base_lba*BLK,begin,&begin) ||
           !mul_checked((uint64_t)high-(uint64_t)low+1u,
                        cyl_bytes,&length) || !add_checked(begin,length,&end)) {
            strcpy(out->status,"RDB partition geometry overflows byte range");
            return -1;
        }
        p=&out->parts[out->count++];
        memset(p,0,sizeof(*p));
        if(end>r->total_bytes) {
            p->outside_media=1;
            backup_range_ok=0;
        }
        if(p->outside_media && !out->rdb_capacity_mismatch) {
            char last_lba[21],media_lbas[21];
            dolly_surface_log_decimal(last_lba,end/BLK-1u);
            dolly_surface_log_decimal(media_lbas,r->total_bytes/BLK);
            snprintf(out->status,sizeof(out->status),
                     "RDB #%u outside media: end LBA %s, disk %s sectors",
                     used,last_lba,media_lbas);
            out->rdb_capacity_mismatch=1;
        }
        safe_label(p->label,r->buffer+37,r->buffer[36]);
        if(!p->label[0])sprintf(p->label,"RDB %lu",(unsigned long)out->count);
        p->is_rdb=1;
        p->rdb_bootable=(unsigned char)((be32(r->buffer+20)&1u)!=0);
        p->rdb_automount=(unsigned char)((be32(r->buffer+20)&2u)==0);
        p->start_bytes=begin;p->length_bytes=length;
        p->descriptor_block=head;
        p->low_cyl=low;p->high_cyl=high;
        p->fs_block_bytes=blockwords*4u;
        p->sectors_per_block=be32(r->buffer+144);
        p->reserved_blocks=be32(r->buffer+152);
        p->dos_type=dos;
        /* de_TableSize is the last available DosEnvec index.
         * Never show absent optional fields as literal zeroes. */
        p->dos_type_present=(unsigned char)(envwords>=16u);
        p->boot_priority_present=(unsigned char)(envwords>=15u);
        p->max_transfer_present=(unsigned char)(envwords>=13u);
        p->dma_mask_present=(unsigned char)(envwords>=14u);
        if(p->boot_priority_present)
            p->boot_priority=(int32_t)be32(r->buffer+188);
        if(p->max_transfer_present)
            p->max_transfer=be32(r->buffer+180);
        if(p->dma_mask_present)
            p->dma_mask=be32(r->buffer+184);
        if(envwords>=12u) {
            p->io_hints.present|=DOLLY_IO_BUF_MEM;
            p->io_hints.buffer_memory=be32(r->buffer+176);
        }
        if(p->max_transfer_present) {
            p->io_hints.present|=DOLLY_IO_MAX_TRANSFER;
            p->io_hints.max_transfer=p->max_transfer;
        }
        if(p->dma_mask_present) {
            p->io_hints.present|=DOLLY_IO_MASK;
            p->io_hints.mask=p->dma_mask;
        }
        dos_name(type,dos);
        strcpy(label_copy,p->label);
        /* DosType is RDB filesystem metadata, not a filesystem
         * signature verified from partition contents.
         */
        sprintf(p->summary,
                "%s %luMiB %s%s Cyl%lu-%lu Blk%lu SPB%lu",
                label_copy,(unsigned long)(length/1048576u),type,
                dos==0x444f5301ul?"(FFS)":
                dos==0x444f5300ul?"(OFS)":"",
                (unsigned long)low,(unsigned long)high,
                (unsigned long)p->fs_block_bytes,
                (unsigned long)p->sectors_per_block);
        head=next;
    }
    /* Do not accept a partition covering RDB metadata blocks. */
    for(i=0;i<out->count;++i) {
        DollyNativePartition *part=&out->parts[i];
        uint64_t metadata=((uint64_t)base_lba+
                           (uint64_t)header_block)*BLK;
        if(part->start_bytes<((uint64_t)reserved_hi+1u)*BLK)
            backup_range_ok=0;
        if(metadata>=part->start_bytes &&
           metadata-part->start_bytes<part->length_bytes) {
            strcpy(out->status,"Partition overlaps RDB header");
            return -1;
        }
        for(j=0;j<used;++j) {
            metadata=((uint64_t)base_lba+(uint64_t)seen[j])*BLK;
            if(metadata>=part->start_bytes &&
               metadata-part->start_bytes<part->length_bytes) {
                strcpy(out->status,"Partition overlaps RDB PART");
                return -1;
            }
        }
    }
    /* Fail closed if RDB partitions overlap one another. */
    for(i=0;i<out->count;++i)
        for(j=i+1u;j<out->count;++j) {
            DollyNativePartition *a=&out->parts[i],*b=&out->parts[j];
            if(a->start_bytes<b->start_bytes+b->length_bytes &&
               b->start_bytes<a->start_bytes+a->length_bytes) {
                strcpy(out->status,"Overlapping RDB partitions");
                return -1;
            }
        }
    if(!out->rdb_capacity_mismatch)strcpy(out->status,"RDB");
    out->format=DOLLY_NATIVE_PARTITION_RDB;
    out->rdb_header_block=header_block;
    if(backup_range_ok)out->rdb_backup_blocks=reserved_hi+1u;
    return 1;
}
static int scan_mbr(Reader *r,DollyNativePartitionMap *out,int inventory) {
    unsigned i,j;
    int primary_only=1;
    r->base_lba=0;
    if(!load_sector(r,0)) {
        strcpy(out->status,"MBR read failed");return 0;
    }
    if(r->buffer[510]!=0x55u || r->buffer[511]!=0xaau) {
        if(!load_sector(r,1))strcpy(out->status,"GPT header check failed");
        else if(!memcmp(r->buffer,"EFI PART",8))strcpy(out->status,"GPT unsupported");
        else {
            strcpy(out->status,"No recognized RDB/MBR");
            out->no_partition_table=1;
        }
        return 0;
    }
    for(i=0;i<4u;++i) {
        const UBYTE *p=r->buffer+446+16u*i;
        int container=p[4]==0x05u || p[4]==0x0fu ||
                      p[4]==0x85u || p[4]==0x76u;
        ULONG first=le32(p+8),count=le32(p+12);
        uint64_t begin=(uint64_t)first*BLK;
        uint64_t length=(uint64_t)count*BLK;
        DollyNativePartition *part;
        char label_copy[40];
        if(!p[4] && !first && !count)continue;
        if(p[4]==0xeeu) {
            strcpy(out->status,"Protective MBR / GPT unsupported");
            return 0;
        }
        if(container && !inventory) {
            strcpy(out->status,"Extended or nested RDB MBR unsupported here");
            return 0;
        }
        if(!p[4] || !first || !count || (p[0]!=0 && p[0]!=0x80) ||
           begin>r->total_bytes ||
           length>r->total_bytes-begin) {
            strcpy(out->status,"Invalid MBR partition entry");
            return 0;
        }
        part=&out->parts[out->count++];
        memset(part,0,sizeof(*part));
        sprintf(part->label,"MBR %lu",(unsigned long)(i+1u));
        strcpy(label_copy,part->label);
        part->mbr_type=p[4];
        part->mbr_slot=(UBYTE)(i+1u);
        part->mbr_active=(UBYTE)(p[0]==0x80u);
        part->mbr_container=(UBYTE)container;
        if(container)primary_only=0;
        part->start_bytes=begin;part->length_bytes=length;
        sprintf(part->summary,"%s | %lu MiB | Type %02lx | LBA %lu+%lu | %lu B",
               label_copy,(unsigned long)(length/1048576u),
               (unsigned long)p[4],(unsigned long)first,
               (unsigned long)count,(unsigned long)BLK);
    }
    for(i=0;i<out->count;++i)
        for(j=i+1u;j<out->count;++j) {
            DollyNativePartition *a=&out->parts[i],*b=&out->parts[j];
            if(a->start_bytes<b->start_bytes+b->length_bytes &&
               b->start_bytes<a->start_bytes+a->length_bytes) {
                strcpy(out->status,"Overlapping MBR partitions");
                return 0;
            }
        }
    strcpy(out->status,out->count?"MBR":"Empty MBR");
    if(out->count) {
        out->format=DOLLY_NATIVE_PARTITION_MBR;
        out->mbr_backup_available=(unsigned char)primary_only;
    }
    return out->count>0;
}
static int has_mbr_entries(const UBYTE *sector) {
    return sector[510]==0x55u && sector[511]==0xaau &&
           (sector[450]!=0 || sector[466]!=0 ||
            sector[482]!=0 || sector[498]!=0);
}

static int scan_device(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionMap *out,
          DollyNativePartitionFormat expected,BPTR backup_file,int inventory) {
    struct MsgPort *port=NULL;
    struct IOExtTD *io=NULL;
    Reader r;
    LONG rc;
    int opened=0,ok=0,rs;
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    strcpy(out->status,"Not scanned");
    if(!driver || !geometry || !geometry->capacity_valid ||
       geometry->block_bytes!=BLK || geometry->block_count<16u ||
       strcmp(driver,"trackdisk.device")==0) {
        strcpy(out->status,"Partition scan requires 512 B sectors");
        return 0;
    }
    memset(&r,0,sizeof(r));
    r.total_bytes=geometry->block_count*(uint64_t)geometry->block_bytes;
    port=CreateMsgPort();
    if(!port)goto finish;
    io=(struct IOExtTD *)CreateIORequest(port,sizeof(*io));
    if(!io)goto finish;
    rc=OpenDevice((STRPTR)driver,unit,(struct IORequest *)io,0);
    if(rc)goto finish;
    opened=1;
    r.io=io;
    r.buffer=(UBYTE *)AllocMem(BLK,MEMF_CHIP|MEMF_CLEAR);
    if(!r.buffer)goto finish;
    rs=scan_rdb(&r,out,0ul,16ul);
    if(rs==1) {
        /* Reject ambiguous hybrid RDB and MBR media until a full
         * collision audit can account for both partition schemes.
         */
        if(!load_sector(&r,0ul)) {
            strcpy(out->status,"Cannot verify RDB/MBR coexistence");
        } else if(has_mbr_entries(r.buffer)) {
            strcpy(out->status,"Hybrid RDB/MBR unsupported");
        } else ok=1;
    } else if(rs==0)ok=scan_mbr(&r,out,inventory);
    if(ok && backup_file) {
        ok=out->format==expected;
        if(ok && expected==DOLLY_NATIVE_PARTITION_MBR) {
            /* scan_mbr left the validated LBA 0 in the read buffer. */
            ok=Write(backup_file,r.buffer,BLK)==BLK;
        } else if(ok) {
            ULONG block;
            ok=out->rdb_backup_blocks!=0;
            for(block=0;ok && block<out->rdb_backup_blocks;++block)
                ok=load_sector(&r,block) &&
                    Write(backup_file,r.buffer,BLK)==BLK;
        }
    }
finish:
    if(opened)CloseDevice((struct IORequest *)io);
    if(r.buffer)FreeMem(r.buffer,BLK);
    if(io)DeleteIORequest((struct IORequest *)io);
    if(port)DeleteMsgPort(port);
    if(!ok) {
        out->count=0;
        out->format=DOLLY_NATIVE_PARTITION_NONE;
        out->rdb_backup_blocks=0;
        out->rdb_capacity_mismatch=0;
        out->mbr_backup_available=0;
    }
    if(!opened)strcpy(out->status,"Device open failed");
    return ok;
}
int dolly_amiga_partition_scan(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionMap *out) {
    return scan_device(driver,unit,geometry,out,DOLLY_NATIVE_PARTITION_NONE,0,0);
}
int dolly_amiga_inventory_scan(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionMap *out) {
    return scan_device(driver,unit,geometry,out,DOLLY_NATIVE_PARTITION_NONE,0,1);
}
int dolly_amiga_partition_backup(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionFormat format,
          BPTR file) {
    DollyNativePartitionMap *map;
    int ok;
    if(!file || (format!=DOLLY_NATIVE_PARTITION_RDB &&
                 format!=DOLLY_NATIVE_PARTITION_MBR))return 0;
    map=AllocMem(sizeof(*map),MEMF_PUBLIC);
    if(!map)return 0;
    ok=scan_device(driver,unit,geometry,map,format,file,0);
    FreeMem(map,sizeof(*map));
    return ok;
}

/* Validate the snapshot with the same bounded parser as Inventory. Only
 * RDSK/PART sectors may change, and only at their existing locations. */
static int restore_maps_match(const DollyNativePartitionMap *old,
                              const DollyNativePartitionMap *replacement) {
    unsigned i;
    if(!old->rdb_backup_blocks || old->rdb_backup_blocks!=replacement->rdb_backup_blocks ||
       old->rdb_header_block!=replacement->rdb_header_block ||
       old->count!=replacement->count)return 0;
    for(i=0;i<old->count;++i)
        if(old->parts[i].descriptor_block!=replacement->parts[i].descriptor_block ||
           old->parts[i].start_bytes!=replacement->parts[i].start_bytes ||
           old->parts[i].length_bytes!=replacement->parts[i].length_bytes ||
           old->parts[i].dos_type_present!=replacement->parts[i].dos_type_present ||
           old->parts[i].dos_type!=replacement->parts[i].dos_type)return 0;
    return 1;
}

static int restore_unit_idle(const char *driver,ULONG unit,const struct IOStdReq *io) {
    return dolly_amiga_unit_registered(driver,unit)==0 && io->io_Device &&
           io->io_Device->dd_Library.lib_OpenCnt==1;
}

static int restore_sector_matches(Reader *reader,ULONG block,const UBYTE *expected) {
    unsigned i;
    /* A read that reports success without filling the DMA buffer must fail. */
    for(i=0;i<BLK;++i)reader->buffer[i]=expected[i]^0xffu;
    return load_sector(reader,block) && memcmp(reader->buffer,expected,BLK)==0;
}

const char *dolly_amiga_restore_rdb(struct Window *parent,const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,ULONG blocks,BPTR backup,
          DollyRdbRecoverySink save_recovery,void *context) {
    struct MsgPort *port=NULL;
    struct IOExtTD *request=NULL;
    DollyNativePartitionMap *old=NULL,*replacement=NULL;
    DollyAmigaWriteTarget target;
    DollyMediaGeometry live={0};
    Reader reader={0};
    UBYTE *bytes=NULL,*previous=NULL;
    ULONG size,i,header=RDB_END;
    unsigned pass;
    int opened=0,changed=0;
    char unit_name[112],size_text[48],warning[160];
    const char *failure="Restore RDB requires an offline ROM scsi.device unit with 512 B sectors.";
    if(!parent || !driver || !geometry || !backup || !save_recovery ||
       !geometry->capacity_valid ||
       geometry->block_bytes!=BLK || blocks<16 || blocks>DOLLY_RDB_RESTORE_MAX_BLOCKS ||
       blocks>geometry->block_count || strcmp(driver,"scsi.device")!=0 ||
       dolly_amiga_unit_registered(driver,unit)!=0 ||
       !dolly_amiga_probe_rom_scsi(unit))return failure;
    size=blocks*BLK;
    failure="Cannot verify current target capacity.";
    if(!dolly_amiga_device_query(driver,unit,unit_name,sizeof(unit_name),size_text,
                                 sizeof(size_text),&live,1,NULL,0) ||
       !live.capacity_valid || live.block_bytes!=BLK ||
       live.block_count!=geometry->block_count ||
       live.block_count>UINT64_MAX/BLK)return failure;
    failure="Not enough memory or could not open the target.";
    old=calloc(1,sizeof(*old));replacement=calloc(1,sizeof(*replacement));
    bytes=malloc(size);previous=malloc(size);
    port=CreateMsgPort();
    reader.buffer=AllocMem(BLK,MEMF_CHIP|MEMF_PUBLIC);
    if(!old || !replacement || !bytes || !previous || !port || !reader.buffer)goto out;
    request=(struct IOExtTD *)CreateIORequest(port,sizeof(*request)+128u);
    if(!request || OpenDevice((STRPTR)driver,unit,(struct IORequest *)request,0))goto out;
    opened=1;
    reader.io=request;
    reader.total_bytes=live.block_count*BLK;
    failure="Target is registered in AmigaDOS or the device is already in use.";
    if(!restore_unit_idle(driver,unit,&request->iotd_Req))goto out;
    failure="Backup size or read error. Expected the complete reserved RDB area.";
    if(Read(backup,bytes,size)!=(LONG)size || Read(backup,reader.buffer,1)!=0)goto out;
    failure="Cannot read current RDB sectors.";
    for(i=0;i<blocks;++i) {
        if(!load_sector(&reader,i))goto out;
        memcpy(previous+i*BLK,reader.buffer,BLK);
    }
    failure="Restore RDB cannot be used with a hybrid RDB/MBR.";
    if(has_mbr_entries(previous) || has_mbr_entries(bytes))goto out;
    reader.snapshot_bytes=size;
    reader.snapshot=previous;
    failure="Current RDB is invalid or exceeds the backup area.";
    if(scan_rdb(&reader,old,0,16)!=1 || old->rdb_backup_blocks!=blocks)goto out;
    reader.snapshot=bytes;
    failure="Backup RDB is invalid, or partition extents or DosTypes differ.";
    if(scan_rdb(&reader,replacement,0,16)!=1 || !restore_maps_match(old,replacement))goto out;
    failure="Only existing RDSK/PART metadata may change in this build.";
    for(i=0;i<blocks;++i) {
        const UBYTE *a=previous+i*BLK,*b=bytes+i*BLK;
        if(memcmp(a,b,BLK)==0)continue;
        if(memcmp(a,b,4)!=0)goto out;
        if(memcmp(a,"RDSK",4)==0 && i==old->rdb_header_block) {
            /* Keep physical geometry, reserved bounds, and top-level links. */
            if(header!=RDB_END || be32(a+4)<39u || be32(b+4)<39u ||
               memcmp(a+12,b+12,148)!=0 ||
               memcmp(a+216,b+216,BLK-216)!=0)goto out;
            header=i;
        } else {
            unsigned part;
            for(part=0;part<old->count;++part)
                if(old->parts[part].descriptor_block==i)break;
            if(part==old->count || memcmp(a,"PART",4)!=0 ||
               !checksum(a,45) || !checksum(b,45) ||
               be32(a+16)!=be32(b+16) ||
               memcmp(a+132,b+132,32)!=0)goto out;
        }
        changed=1;
    }
    if(!changed)goto success;
    snprintf(warning,sizeof(warning),"Restore RDB on %s / Unit %lu.\n"
        "Reserved range: LBA 0-%lu. Partition extents stay the same.",
        driver,(unsigned long)unit,(unsigned long)(blocks-1));
    failure="Restore RDB cancelled.";
    if(!dolly_amiga_confirm_destructive(parent,warning))goto out;
    failure="Could not save the current RDB as the recovery copy.";
    if(!save_recovery(context,previous,size))goto out;
    reader.snapshot=NULL;
    failure="Target changed or became busy. Restore stopped before writing.";
    if(!restore_unit_idle(driver,unit,&request->iotd_Req))goto out;
    for(i=0;i<blocks;++i)
        if(!restore_sector_matches(&reader,i,previous+i*BLK))goto out;
    if(!restore_unit_idle(driver,unit,&request->iotd_Req))goto out;
    /* Qualify UPDATE before the first modification. Readback is mandatory. */
    request->iotd_Req.io_Command=CMD_UPDATE;
    request->iotd_Req.io_Flags=0;
    request->iotd_Req.io_Error=0;
    request->iotd_Req.io_Length=0;
    request->iotd_Req.io_Data=NULL;
    request->iotd_Req.io_Offset=0;
    request->iotd_Req.io_Actual=0;
    failure="The target driver did not accept CMD_UPDATE. No RDB sectors written.";
    if(DoIO((struct IORequest *)request) || request->iotd_Req.io_Error)goto out;
    target=(DollyAmigaWriteTarget){&request->iotd_Req,reader.buffer,BLK,
        reader.total_bytes,0,size,BLK,BLK,0x00fffffe,2,0};
    for(pass=0;pass<2;++pass)
        for(i=0;i<blocks;++i) {
            if((pass==1)!=(i==header) || !memcmp(previous+i*BLK,bytes+i*BLK,BLK))
                continue;
            failure="Target became busy during Restore RDB. Restore stopped.\n"
                    "The RDB may be partially updated. Keep the recovery copy.";
            if(!restore_unit_idle(driver,unit,&request->iotd_Req))goto out;
            failure="RDB write or verification failed. The RDB may be partially updated.\n"
                    "Use the recovery copy from an independent recovery environment.";
            if(dolly_amiga_write_batch(&target,(uint64_t)i*BLK,bytes+i*BLK,BLK,1)
                    !=DOLLY_WRITE_VERIFIED)goto out;
        }
    failure="Final RDB verification failed. Keep the recovery copy.";
    for(i=0;i<blocks;++i)
        if(!restore_sector_matches(&reader,i,bytes+i*BLK))goto out;
success:
    failure=NULL;
out:
    if(opened)CloseDevice((struct IORequest *)request);
    if(request)DeleteIORequest((struct IORequest *)request);
    if(port)DeleteMsgPort(port);
    if(reader.buffer)FreeMem(reader.buffer,BLK);
    free(previous);free(bytes);free(replacement);free(old);
    return failure;
}
