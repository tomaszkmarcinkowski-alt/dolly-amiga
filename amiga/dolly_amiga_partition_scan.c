#include "dolly_amiga_partition_scan.h"
#include <exec/io.h>
#include <exec/memory.h>
#include <devices/trackdisk.h>
#include <proto/exec.h>
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
typedef struct Reader {
    struct IOExtTD *io;
    UBYTE *buffer;
    uint64_t total_bytes;
    ULONG base_lba;
    int unreadable;
} Reader;
static int load_sector(Reader *r,ULONG relative_lba) {
    uint64_t lba=(uint64_t)r->base_lba+(uint64_t)relative_lba;
    uint64_t pos=lba*BLK;
    struct IOStdReq *io=&r->io->iotd_Req;
    if(pos>r->total_bytes || BLK>r->total_bytes-pos ||
       pos>0xfffffffful)return 0;
    io->io_Command=CMD_READ;
    io->io_Data=r->buffer;
    io->io_Length=BLK;
    io->io_Offset=(ULONG)pos;
    io->io_Flags=0;
    io->io_Actual=0;
    if(DoIO((struct IORequest *)r->io)!=0 ||
       io->io_Actual!=BLK) {
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
    ULONG block,head=RDB_END,seen[DOLLY_PART_MAX];
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
        break;
    }
    if(head==RDB_END)return 0;
    while(head!=RDB_END) {
        ULONG next,envwords,blockwords,heads,spt,low,high,dos;
        uint64_t cyl_bytes,begin,length;
        DollyNativePartition *p;
        char type[5];
        if(used>=DOLLY_PART_MAX) {
            strcpy(out->status,"Too many RDB partitions");
            return -1;
        }
        for(j=0;j<used;++j)if(seen[j]==head) {
            strcpy(out->status,"Cycle in RDB PART chain");
            return -1;
        }
        seen[used++]=head;
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
        if(!heads || !spt || !blockwords ||
           blockwords>8192ul || high<low) {
            strcpy(out->status,"Invalid RDB geometry");
            return -1;
        }
        cyl_bytes=(uint64_t)heads*(uint64_t)spt*
                  (uint64_t)blockwords*4u;
        begin=(uint64_t)base_lba*BLK+
              (uint64_t)low*cyl_bytes;
        length=((uint64_t)high-(uint64_t)low+1u)*cyl_bytes;
        if(begin>r->total_bytes || !length ||
           length>r->total_bytes-begin) {
            strcpy(out->status,"RDB partition outside media");
            return -1;
        }
        p=&out->parts[out->count++];
        memset(p,0,sizeof(*p));
        safe_label(p->label,r->buffer+37,r->buffer[36]);
        if(!p->label[0])sprintf(p->label,"RDB %u",out->count);
        p->is_rdb=1;
        p->start_bytes=begin;p->length_bytes=length;
        p->low_cyl=low;p->high_cyl=high;
        p->fs_block_bytes=blockwords*4u;
        p->sectors_per_block=be32(r->buffer+144);
        p->dos_type=dos;
        dos_name(type,dos);
        /* DosType is RDB filesystem metadata, not a filesystem
         * signature verified from partition contents.
         */
        sprintf(p->summary,
                "%s %luMiB %s%s Cyl%lu-%lu Blk%lu SPB%lu",
                p->label,(unsigned long)(length/1048576u),type,
                dos==0x444f5301ul?"(FFS)":
                dos==0x444f5300ul?"(OFS)":"",
                (unsigned long)low,(unsigned long)high,
                (unsigned long)p->fs_block_bytes,
                (unsigned long)p->sectors_per_block);
        head=next;
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
    strcpy(out->status,"RDB");
    return 1;
}
static int scan_mbr(Reader *r,DollyNativePartitionMap *out) {
    unsigned i,j;
    r->base_lba=0;
    if(!load_sector(r,0)) {
        strcpy(out->status,"MBR read failed");return 0;
    }
    if(r->buffer[510]!=0x55u || r->buffer[511]!=0xaau) {
        strcpy(out->status,"No recognized RDB/MBR");return 0;
    }
    for(i=0;i<4u;++i) {
        const UBYTE *p=r->buffer+446+16u*i;
        ULONG first=le32(p+8),count=le32(p+12);
        uint64_t begin=(uint64_t)first*BLK;
        uint64_t length=(uint64_t)count*BLK;
        DollyNativePartition *part;
        if(!p[4] && !first && !count)continue;
        if(!first || !count || (p[0]!=0 && p[0]!=0x80) ||
           p[4]==0xeeu || p[4]==0x05u || p[4]==0x0fu ||
           p[4]==0x85u || p[4]==0x76u ||
           begin>r->total_bytes ||
           length>r->total_bytes-begin) {
            strcpy(out->status,"Unsupported MBR/GPT/extended/nested RDB");
            return 0;
        }
        part=&out->parts[out->count++];
        memset(part,0,sizeof(*part));
        sprintf(part->label,"MBR %u",i+1u);
        part->mbr_type=p[4];
        part->start_bytes=begin;part->length_bytes=length;
        part->fs_block_bytes=BLK;
        sprintf(part->summary,"%s | %lu MiB | Type %02lx | LBA %lu+%lu | %lu B",
               part->label,(unsigned long)(length/1048576u),
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
    return out->count>0;
}
int dolly_amiga_partition_scan(const char *driver,ULONG unit,
          const DollyMediaGeometry *geometry,DollyNativePartitionMap *out) {
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
    if(rs==1)ok=1;
    else if(rs==0)ok=scan_mbr(&r,out);
finish:
    if(opened)CloseDevice((struct IORequest *)io);
    if(r.buffer)FreeMem(r.buffer,BLK);
    if(io)DeleteIORequest((struct IORequest *)io);
    if(port)DeleteMsgPort(port);
    if(!ok)out->count=0;
    if(!opened)strcpy(out->status,"Device open failed");
    return ok;
}
