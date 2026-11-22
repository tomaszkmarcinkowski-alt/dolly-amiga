#include "dolly_apple_media.h"
#include <stdlib.h>
#include <string.h>

#define SECTOR 512u
#define HFS 0x4244u
#define HFS_PLUS 0x482bu
#define HFSX 0x4858u
#define DRIVER_DESCRIPTOR 0x4552u
#define PARTITION_MAP 0x504du

static uint16_t be16(const unsigned char *data) {
    return (uint16_t)((uint16_t)data[0]<<8 | data[1]);
}

static uint32_t be32(const unsigned char *data) {
    return (uint32_t)be16(data)<<16 | be16(data+2);
}

static int fits(uint64_t start,uint64_t size,uint64_t bound) {
    return start<=bound && size<=bound-start;
}

static int read_sector(DollyBlockSource *source,uint64_t offset,unsigned char data[SECTOR]) {
    return dolly_block_read(source,offset,data,SECTOR).status==DOLLY_BLOCK_OK;
}

static DollyAppleMedia probe_volume(DollyBlockSource *source,uint64_t start,uint64_t bytes) {
    unsigned char header[SECTOR];
    uint16_t signature,embedded=0;
    if(bytes<3u*SECTOR)return DOLLY_APPLE_UNKNOWN;
    if(!read_sector(source,start+2u*SECTOR,header))return DOLLY_APPLE_UNREADABLE;
    signature=be16(header);
    if(signature==HFS) {
        uint32_t blocks=be16(header+18),block_size=be32(header+20);
        uint32_t bitmap_sector=be16(header+14),allocation_sector=be16(header+28);
        uint64_t allocation_start=(uint64_t)allocation_sector*SECTOR;
        if(!blocks || block_size<SECTOR || block_size%SECTOR || bitmap_sector<3u ||
           bitmap_sector+(blocks+4095u)/4096u>allocation_sector ||
           !fits(allocation_start,(uint64_t)blocks*block_size,bytes))return DOLLY_APPLE_INVALID;
        embedded=be16(header+124);
        if(embedded!=HFS_PLUS && embedded!=HFSX)return DOLLY_APPLE_HFS;
        uint32_t first=be16(header+126),count=be16(header+128);
        bytes=(uint64_t)count*block_size;
        if(!fits(first,count,blocks) || bytes<3u*SECTOR)return DOLLY_APPLE_INVALID;
        if(!read_sector(source,start+allocation_start+(uint64_t)first*block_size+2u*SECTOR,
                        header))return DOLLY_APPLE_UNREADABLE;
        signature=be16(header);
        if(signature!=embedded)return DOLLY_APPLE_INVALID;
    }
    if(signature==HFS_PLUS || signature==HFSX) {
        uint32_t block_size=be32(header+40),blocks=be32(header+44);
        uint64_t volume_bytes=(uint64_t)block_size*blocks;
        if(be16(header+2)!=(signature==HFS_PLUS?4u:5u) || block_size<SECTOR ||
           (block_size&(block_size-1u)) || volume_bytes<3u*SECTOR || volume_bytes>bytes ||
           be32(header+48)>blocks)return DOLLY_APPLE_INVALID;
        if(signature==HFSX)return DOLLY_APPLE_HFSX;
        return embedded?DOLLY_APPLE_WRAPPED_HFS_PLUS:DOLLY_APPLE_HFS_PLUS;
    }
    return DOLLY_APPLE_UNKNOWN;
}

static DollyAppleMedia probe_map(DollyBlockSource *source,uint64_t start,uint64_t sectors,
                                 unsigned char entry[SECTOR],DollyAppleMap *map,
                                 uint64_t *largest_partition) {
    struct MapExtent { uint32_t first,count; } extents[DOLLY_APPLE_MAP_LIMIT];
    uint32_t entries=be32(entry+4);
    unsigned volumes=0;
    int found_map=0;
    if(!entries || entries>=sectors)return DOLLY_APPLE_INVALID;
    if(entries>DOLLY_APPLE_MAP_LIMIT)return DOLLY_APPLE_UNSUPPORTED;
    for(uint32_t i=0;i<entries;++i) {
        if(i && !read_sector(source,start+(uint64_t)(i+1u)*SECTOR,entry))
            return DOLLY_APPLE_UNREADABLE;
        uint32_t first=be32(entry+8),count=be32(entry+12);
        uint32_t data_start=be32(entry+80),data_count=be32(entry+84);
        if(be16(entry)!=PARTITION_MAP || be32(entry+4)!=entries || !first || !count ||
           !fits(first,count,sectors) || !fits(data_start,data_count,count))
            return DOLLY_APPLE_INVALID;
        if(!memcmp(entry+48,"Apple_partition_map",sizeof("Apple_partition_map"))) {
            if(found_map || first!=1u || count<entries)return DOLLY_APPLE_INVALID;
            found_map=1;
        }
        for(uint32_t j=0;j<i;++j) {
            if((uint64_t)first+count>extents[j].first &&
               (uint64_t)extents[j].first+extents[j].count>first)
                return DOLLY_APPLE_INVALID;
        }
        extents[i]=(struct MapExtent){first,count};
        uint64_t partition_bytes=(uint64_t)count*SECTOR;
        if(largest_partition && partition_bytes>*largest_partition &&
           memcmp(entry+48,"Apple_Free",sizeof("Apple_Free")))
            *largest_partition=partition_bytes;
        if(!memcmp(entry+48,"Apple_HFS",sizeof("Apple_HFS"))) {
            if(data_count<3u)return DOLLY_APPLE_INVALID;
            uint64_t offset=((uint64_t)first+data_start)*SECTOR;
            uint64_t bytes=(uint64_t)data_count*SECTOR;
            DollyAppleMedia volume=probe_volume(source,start+offset,bytes);
            if(volume==DOLLY_APPLE_INVALID || volume==DOLLY_APPLE_UNREADABLE)return volume;
            if(map && volume!=DOLLY_APPLE_UNKNOWN) {
                DollyAppleVolume *result=&map->volumes[volumes++];
                *result=(DollyAppleVolume){.offset=offset,.bytes=bytes,.partition_bytes=partition_bytes,
                                           .entry=i+1u,.media=volume};
                for(unsigned c=0;c<32u && entry[16u+c];++c) {
                    unsigned char value=entry[16u+c];
                    result->name[c]=value>=32u && value<=126u?(char)value:'?';
                }
            }
        }
    }
    if(!found_map)return DOLLY_APPLE_INVALID;
    if(map)map->count=volumes;
    return DOLLY_APPLE_APM;
}

static DollyAppleMedia inspect_media(DollyBlockSource *source,uint64_t start,uint64_t bytes,
                                     DollyAppleMap *map,uint64_t *largest_partition) {
    unsigned char header[SECTOR];
    uint64_t sectors=bytes/SECTOR;
    if(map)map->count=0;
    if(!source || start%SECTOR || bytes%SECTOR || !fits(start,bytes,dolly_block_size(source)))
        return DOLLY_APPLE_INVALID;
    if(sectors<2u)return DOLLY_APPLE_UNKNOWN;
    if(!read_sector(source,start,header))return DOLLY_APPLE_UNREADABLE;
    int has_driver_descriptor=be16(header)==DRIVER_DESCRIPTOR;
    if(has_driver_descriptor) {
        uint16_t drivers=be16(header+16);
        if(be16(header+2)!=SECTOR)return DOLLY_APPLE_UNSUPPORTED;
        uint32_t declared=be32(header+4);
        if(declared<2u || declared>sectors || drivers>(SECTOR-18u)/8u)return DOLLY_APPLE_INVALID;
        sectors=declared;
        for(unsigned i=0;i<drivers;++i) {
            const unsigned char *driver=header+18u+8u*i;
            if(!be32(driver) || !be16(driver+4) || !fits(be32(driver),be16(driver+4),sectors))
                return DOLLY_APPLE_INVALID;
        }
    }
    if(!read_sector(source,start+SECTOR,header))return DOLLY_APPLE_UNREADABLE;
    if(be16(header)==PARTITION_MAP)return probe_map(source,start,sectors,header,map,largest_partition);
    if(has_driver_descriptor)return DOLLY_APPLE_INVALID;
    return probe_volume(source,start,bytes);
}

DollyAppleMedia dolly_apple_media_inspect(DollyBlockSource *source,uint64_t start,uint64_t bytes,
                                         DollyAppleMap *map) {
    return inspect_media(source,start,bytes,map,NULL);
}

DollyAppleMedia dolly_apple_media_probe(DollyBlockSource *source,uint64_t start,uint64_t bytes) {
    return dolly_apple_media_inspect(source,start,bytes,NULL);
}

int dolly_apple_media_is_volume(DollyAppleMedia media) {
    return media>=DOLLY_APPLE_HFS && media<=DOLLY_APPLE_HFSX;
}

int dolly_shapeshifter_file_fits(uint64_t bytes) {
    return bytes && !(bytes%SECTOR) && bytes<=INT32_MAX;
}

int dolly_shapeshifter_partition_fits(uint64_t start,uint64_t bytes) {
    return bytes && !(start%SECTOR) && !(bytes%SECTOR) &&
           fits(start,bytes,UINT64_C(0x100000000));
}

DollyAppleMedia dolly_shapeshifter_media_limits(DollyBlockSource *source,uint64_t start,
                                               uint64_t bytes,int *volume_too_large) {
    uint64_t largest=0;
    DollyAppleMedia media=inspect_media(source,start,bytes,NULL,&largest);
    if(volume_too_large)
        *volume_too_large=(media==DOLLY_APPLE_APM && largest && !dolly_shapeshifter_file_fits(largest)) ||
                          (dolly_apple_media_is_volume(media) && !dolly_shapeshifter_file_fits(bytes));
    return media;
}

const char *dolly_apple_media_name(DollyAppleMedia media) {
    switch(media) {
    case DOLLY_APPLE_HFS:return "HFS Standard";
    case DOLLY_APPLE_HFS_PLUS:return "HFS Plus";
    case DOLLY_APPLE_WRAPPED_HFS_PLUS:return "HFS Plus (HFS wrapper)";
    case DOLLY_APPLE_HFSX:return "HFSX";
    case DOLLY_APPLE_APM:return "Apple Partition Map disk";
    case DOLLY_APPLE_INVALID:return "Invalid Apple media layout";
    case DOLLY_APPLE_UNSUPPORTED:return "Apple media layout outside probe limits";
    case DOLLY_APPLE_UNREADABLE:return "Cannot read Apple media metadata";
    default:return "Unknown";
    }
}

enum { TEMPLATE_MAP_ENTRIES=3, TEMPLATE_HEADER_BYTES=4u*SECTOR };

typedef struct DollyAppleDisk {
    DollyBlockSource *volume,*driver_template;
    uint64_t driver_start,driver_end,volume_start;
    unsigned char header[TEMPLATE_HEADER_BYTES];
} DollyAppleDisk;

static void put32(unsigned char *data,uint32_t value) {
    data[0]=(unsigned char)(value>>24);
    data[1]=(unsigned char)(value>>16);
    data[2]=(unsigned char)(value>>8);
    data[3]=(unsigned char)value;
}

static DollyBlockReadResult read_apple_disk(void *opaque,uint64_t offset,void *buffer,size_t bytes) {
    DollyAppleDisk *disk=opaque;
    DollyBlockReadResult result={DOLLY_BLOCK_OK,0};
    while(result.bytes_read<bytes) {
        unsigned char *out=(unsigned char *)buffer+result.bytes_read;
        size_t count=bytes-result.bytes_read;
        if(offset<sizeof(disk->header)) {
            if(count>sizeof(disk->header)-offset)count=(size_t)(sizeof(disk->header)-offset);
            memcpy(out,disk->header+(size_t)offset,count);
        } else if(offset>=disk->volume_start ||
                  (offset>=disk->driver_start && offset<disk->driver_end)) {
            DollyBlockReadResult read;
            if(offset>=disk->volume_start)
                read=dolly_block_read(disk->volume,offset-disk->volume_start,out,count);
            else {
                if(count>disk->driver_end-offset)count=(size_t)(disk->driver_end-offset);
                read=dolly_block_read(disk->driver_template,offset,out,count);
            }
            result.bytes_read+=read.bytes_read;
            if(read.status!=DOLLY_BLOCK_OK) {
                result.status=read.status;
                return result;
            }
            offset+=count;
            continue;
        } else {
            uint64_t end=offset<disk->driver_start?disk->driver_start:disk->volume_start;
            if(count>end-offset)count=(size_t)(end-offset);
            memset(out,0,count);
        }
        result.bytes_read+=count;
        offset+=count;
    }
    return result;
}

const char *dolly_apple_disk_from_filedisk(DollyBlockSource *volume,DollyBlockSource *driver_template,
                                          DollyBlockSource **out) {
    if(!out)return "Missing Apple disk output.";
    *out=NULL;
    if(!volume || !driver_template)return "A FileDisk and an Apple driver template are required.";
    uint64_t volume_bytes=dolly_block_size(volume);
    if(!dolly_shapeshifter_file_fits(volume_bytes))
        return "A ShapeShifter FileDisk must use whole sectors and be smaller than 2 GiB.";
    DollyAppleMedia format=dolly_apple_media_probe(volume,0,volume_bytes);
    if(!dolly_apple_media_is_volume(format))
        return "Expected a flat HFS-family FileDisk with valid sector geometry.";
    if(dolly_apple_media_probe(driver_template,0,dolly_block_size(driver_template))!=DOLLY_APPLE_APM)
        return "Driver template has no supported Apple Partition Map.";
    DollyAppleDisk *disk=calloc(1,sizeof(*disk));
    if(!disk)return "Cannot allocate the Apple disk header.";
    const char *error="Cannot read the driver template headers.";
    if(dolly_block_read(driver_template,0,disk->header,sizeof(disk->header)).status!=DOLLY_BLOCK_OK)
        goto failed;
    unsigned char *descriptor=disk->header;
    const unsigned char *map=disk->header+SECTOR;
    const unsigned char *driver=disk->header+2u*SECTOR;
    unsigned char *hfs=disk->header+3u*SECTOR;
    uint32_t driver_start=be32(driver+8),driver_sectors=be32(driver+12),hfs_start=be32(hfs+8);
    uint32_t described_start=be32(descriptor+18),described_sectors=be16(descriptor+22);
    const uint64_t volume_sectors=volume_bytes/SECTOR;
    unsigned char driver_code[SECTOR];
    error="Use a template with one Macintosh driver and three entries: map, driver and Apple_HFS.";
    if(be16(descriptor)!=DRIVER_DESCRIPTOR || be16(descriptor+16)!=1u || be16(descriptor+24)!=1u ||
       be32(map+4)!=TEMPLATE_MAP_ENTRIES ||
       memcmp(map+48,"Apple_partition_map",sizeof("Apple_partition_map")) ||
       (memcmp(driver+48,"Apple_Driver43",sizeof("Apple_Driver43")) &&
        memcmp(driver+48,"Apple_Driver",sizeof("Apple_Driver"))) ||
       memcmp(hfs+48,"Apple_HFS",sizeof("Apple_HFS")) || be32(hfs+80) ||
       driver_start<TEMPLATE_MAP_ENTRIES+1u || !fits(driver_start,driver_sectors,hfs_start) ||
       !described_sectors || described_start<driver_start ||
       !fits(described_start-driver_start,described_sectors,driver_sectors))goto failed;
    error="The template's Macintosh driver is empty or unreadable.";
    if(!read_sector(driver_template,(uint64_t)described_start*SECTOR,driver_code))goto failed;
    unsigned used=0;
    while(used<sizeof(driver_code) && !driver_code[used])++used;
    if(used==sizeof(driver_code))goto failed;
    error="The volume and Apple driver area exceed the Apple disk capacity field.";
    if(volume_sectors>UINT32_MAX-hfs_start)goto failed;

    disk->volume=volume;
    disk->driver_template=driver_template;
    disk->driver_start=(uint64_t)driver_start*SECTOR;
    disk->driver_end=((uint64_t)driver_start+driver_sectors)*SECTOR;
    disk->volume_start=(uint64_t)hfs_start*SECTOR;
    put32(descriptor+4,hfs_start+(uint32_t)volume_sectors);
    put32(hfs+12,(uint32_t)volume_sectors);
    memset(hfs+16,0,32);
    memcpy(hfs+16,"Macintosh",sizeof("Macintosh"));
    put32(hfs+84,(uint32_t)volume_sectors);
    /* Discard boot metadata carried by the old volume entry. The FileDisk
     * supplies its own boot blocks while the driver entry remains intact. */
    memset(hfs+92,0,SECTOR-92u);
    error="Cannot open the generated Apple disk view.";
    if(dolly_block_open_backend(disk->volume_start+volume_bytes,read_apple_disk,free,disk,out)
       ==DOLLY_BLOCK_OK)return NULL;
failed:
    free(disk);
    return error;
}
