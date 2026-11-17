#include "dolly_amiga_partition_query.h"
#include "dolly_partition_map.h"
#include <devices/trackdisk.h>
#include <exec/io.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct MediaReader {
    struct MsgPort *port;
    struct IOExtTD *io;
    UBYTE *dma;
    int opened;
} MediaReader;
static DollyBlockReadResult read_sector(void *ctx,uint64_t offset,
                                        void *out,size_t len) {
    MediaReader *m=(MediaReader *)ctx;
    DollyBlockReadResult result={DOLLY_BLOCK_EUNSUPPORTED,0};
    size_t done=0;
    if(!m || !out || !m->opened || (offset%512u) ||
       (len%512u) || offset>UINT32_MAX)return result;
    while(done<len) {
        if(offset+(uint64_t)done>UINT32_MAX)break;
        m->io->iotd_Req.io_Command=CMD_READ;
        m->io->iotd_Req.io_Data=m->dma;
        m->io->iotd_Req.io_Length=512;
        m->io->iotd_Req.io_Offset=(ULONG)(offset+done);
        m->io->iotd_Req.io_Flags=0;
        m->io->iotd_Req.io_Actual=0;
        if(DoIO((struct IORequest *)m->io)!=0 ||
           m->io->iotd_Req.io_Actual!=512)break;
        memcpy((UBYTE *)out+done,m->dma,512);
        done+=512;
    }
    result.bytes_read=done;
    result.status=done==len?DOLLY_BLOCK_OK:DOLLY_BLOCK_EIO;
    return result;
}
static void noop_close(void *ctx) {(void)ctx;}
static void close_reader(MediaReader *m) {
    if(m->opened)CloseDevice((struct IORequest *)m->io);
    if(m->dma)FreeMem(m->dma,512);
    if(m->io)DeleteIORequest((struct IORequest *)m->io);
    if(m->port)DeleteMsgPort(m->port);
}
static void fs_label(char *out,uint32_t fs) {
    unsigned version=fs&255u;
    if((fs>>8)==0x444f53u && version<=7u) {
        static const char *const names[]={
            "OFS","FFS","OFS-Intl","FFS-Intl",
            "OFS-DirC","FFS-DirC","OFS-LFN","FFS-LFN"
        };
        strcpy(out,names[version]);
    } else {
        sprintf(out,"%08lx",(unsigned long)fs);
    }
}
static void add_rdb(DollyGuiPartitionResult *out,const DollyRdb *rdb,
                    const char *prefix) {
    unsigned i;
    for(i=0;i<rdb->partition_count;++i) {
        const DollyPartition *p=&rdb->partitions[i];
        DollyGuiPartition *d;
        char fs[16];
        ULONG block_bytes;
        if(out->count>=DOLLY_GUI_PARTITIONS) {
            out->truncated=1;
            continue;
        }
        d=&out->entries[out->count++];
        d->start_bytes=p->start_bytes;
        d->length_bytes=p->length_bytes;
        if(prefix && *prefix)
            sprintf(d->label,"%s%s",prefix,p->name[0]?p->name:"(unnamed)");
        else
            sprintf(d->label,"%s",p->name[0]?p->name:"(unnamed)");
        fs_label(fs,p->dos_type);
        block_bytes=(ULONG)p->block_size_longwords*4ul;
        sprintf(d->details,
                 "%.12s %luMiB %s LC%lu HC%lu Blk%luB",
                 p->name[0]?p->name:"(unnamed)",
                 (unsigned long)(p->length_bytes/1048576u),fs,
                 (unsigned long)p->low_cylinder,
                 (unsigned long)p->high_cylinder,
                 (unsigned long)block_bytes);
    }
}
static void add_mbr(DollyGuiPartitionResult *out,
                    const DollyMbrPartition *part,unsigned index,
                    ULONG device_block) {
    DollyGuiPartition *d;
    if(out->count>=DOLLY_GUI_PARTITIONS) {
        out->truncated=1;return;
    }
    d=&out->entries[out->count++];
    d->start_bytes=part->start_bytes;
    d->length_bytes=part->length_bytes;
    sprintf(d->label,"MBR %lu (0x%02lx)",
            (unsigned long)(index+1u),(unsigned long)part->type);
    sprintf(d->details,
             "%.12s %luMiB MBR/0x%02lx LC/HC:N/A Sec%luB",
             d->label,(unsigned long)(part->length_bytes/1048576u),
             (unsigned long)part->type,(unsigned long)device_block);
}
int dolly_amiga_partition_query(const char *driver,ULONG unit,
                                uint64_t capacity,uint32_t sector_bytes,
                                DollyGuiPartitionResult *out) {
    MediaReader reader;
    DollyBlockSource *src=NULL;
    DollyPartitionMap *map=NULL;
    DollyMapStatus status;
    unsigned i;
    int success=0;
    if(!out)return 0;
    memset(out,0,sizeof(*out));
    strcpy(out->status,"Partition scan unavailable");
    if(!driver || !*driver || !strcmp(driver,"trackdisk.device"))
        return 0;
    if(!capacity || sector_bytes!=512u) {
        strcpy(out->status,"Requires verified 512-byte sectors");
        return 0;
    }
    memset(&reader,0,sizeof(reader));
    reader.port=CreateMsgPort();
    if(!reader.port)goto done;
    reader.io=(struct IOExtTD *)CreateIORequest(reader.port,
                                                sizeof(*reader.io));
    if(!reader.io)goto done;
    if(OpenDevice((STRPTR)driver,unit,
                  (struct IORequest *)reader.io,0))goto done;
    reader.opened=1;
    reader.dma=(UBYTE *)AllocMem(512,MEMF_CHIP|MEMF_CLEAR);
    if(!reader.dma)goto done;
    if(dolly_block_open_backend(capacity,read_sector,noop_close,
                                &reader,&src)!=DOLLY_BLOCK_OK)goto done;
    /* Large parser state lives on heap, never on the small m68k
     * application stack. No allocations proportional to disk size.
     */
    map=(DollyPartitionMap *)malloc(sizeof(*map));
    if(!map)goto done;
    status=dolly_partition_map_scan(src,sector_bytes,map);
    if(status!=DOLLY_MAP_OK) {
        strncpy(out->status,dolly_map_status_name(status),
                sizeof(out->status)-1u);
        out->status[sizeof(out->status)-1u]='\0';
        goto done;
    }
    if(map->hybrid_mbr_overlap || map->suspect_whole_disk_rdb) {
        strcpy(out->status,"Ambiguous RDB/MBR layout");
        goto done;
    }
    if(map->has_whole_disk_rdb)
        add_rdb(out,&map->whole_disk_rdb,"");
    for(i=0;i<map->partition_count;++i) {
        if(map->primary[i].contains_rdb)
            add_rdb(out,&map->primary[i].nested_rdb,"RDB:");
        else
            add_mbr(out,&map->primary[i],i,sector_bytes);
    }
    if(out->count) {
        out->valid=1;
        strcpy(out->status,out->truncated?
               "Showing first 16 partitions":"Partitions discovered");
        success=1;
    } else strcpy(out->status,"No partitions in map");
done:
    if(map)free(map);
    if(src)dolly_block_close(src);
    close_reader(&reader);
    return success;
}
