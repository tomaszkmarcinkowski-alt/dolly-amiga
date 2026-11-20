#include "dolly_amiga_image.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_endpoint_picker.h"
#include "dolly_amiga_transfer_guard.h"
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern struct DosLibrary *DOSBase;

#define SPLIT_PART_BYTES (UINT32_C(256)*1024*1024)

typedef struct DollyImageDrawer {
    struct DollyImageDrawer *previous;
    BPTR lock;
    uint32_t first,last;
    int published;
} DollyImageDrawer;

static int select_split(DollyWarning *warning) {
    DollyAmigaImage *image=warning->context;
    image->split=1;
    image->format_dly=1;
    return 1;
}

static int check_file_destination(DollyAmigaImage *image,BPTR drawer,DollyWarnings *warnings,
                                  uint64_t payload,uint64_t encoded,
                                  uint64_t single_bytes,uint64_t split_parts) {
    struct InfoData info={0};
    int known=Info(drawer,&info)!=0;
    char message[420],required[21],capacity[21],available[21],missing[21],filesystem[24];
    uint64_t total,free_bytes;
    DollyWarning *warning;
    if(known && info.id_DiskState==ID_WRITE_PROTECTED) {
        image->failure="The destination filesystem is write protected.";
        return 0;
    }
    image->failure="Could not collect image destination warnings.";
    if(split_parts) {
        char count[21];
        dolly_surface_log_decimal(required,single_bytes);
        dolly_surface_log_decimal(count,split_parts);
        if(known)
            snprintf(filesystem,sizeof(filesystem),"0x%08lX",(unsigned long)(uint32_t)info.id_DiskType);
        else strcpy(filesystem,"unknown");
        snprintf(message,sizeof(message),
             "Destination: AmigaDOS V%u, filesystem %s. %s%s bytes. "
             "%s The image must be split into %s IFF DOLY parts with checksums. %s"
             "Confirm to continue, or Cancel.",
             (unsigned)DOSBase->dl_lib.lib_Version,filesystem,
             image->codec?"A single image could reach ":"A single image would require ",required,
             single_bytes>INT32_MAX?
                 "The single-file limit is 2 GiB minus 1 byte.":
                 "The single-file IFF DOLY chunk limit would be exceeded.",
             count,image->format_dly?"":"The output will use uncompressed IFF DOLY. ");
        warning=dolly_warning_add(warnings,"Image must be split",message,"Confirm split");
        if(!warning)return 0;
        warning->action=select_split;
        warning->context=image;
    }
    if(!known || info.id_BytesPerBlock<=0 || info.id_NumBlocks<=0 ||
       info.id_NumBlocksUsed<0 || info.id_NumBlocksUsed>info.id_NumBlocks)
        return dolly_warning_add(warnings,"Free space unknown",
            "Destination: the filesystem did not report usable capacity and free space. "
            "You can continue, but writing will stop if the destination runs out of space.","Continue")!=NULL;
    total=(uint64_t)(uint32_t)info.id_NumBlocks*(uint32_t)info.id_BytesPerBlock;
    free_bytes=(uint64_t)(uint32_t)(info.id_NumBlocks-info.id_NumBlocksUsed)*(uint32_t)info.id_BytesPerBlock;
    if(free_bytes>=encoded)return 1;
    dolly_surface_log_decimal(required,image->codec?payload:encoded);
    dolly_surface_log_decimal(capacity,total);
    dolly_surface_log_decimal(available,free_bytes);
    dolly_surface_log_decimal(missing,encoded-free_bytes);
    snprintf(message,sizeof(message),
             "Destination: image size %s%s bytes. Volume capacity %s bytes. "
             "Free space %s bytes. Short by %s%s bytes%s. "
             "Continue if you can free space before the destination fills, or Cancel.",
             image->codec?"up to ":"",required,capacity,available,
             image->codec?"up to ":"",missing,(image->format_dly || split_parts)?" including IFF overhead":"");
    return dolly_warning_add(warnings,"Insufficient free space",message,"Continue")!=NULL;
}

static int unused_name(const char *name) {
    BPTR lock=Lock((STRPTR)name,ACCESS_READ);
    if(lock) {
        UnLock(lock);
        return 0;
    }
    return IoErr()==ERROR_OBJECT_NOT_FOUND;
}
static int write_image(void *context,const void *data,size_t size) {
    DollyAmigaImage *image=context;
    return Write(image->file,(APTR)data,(LONG)size)==(LONG)size;
}
static int seek_image(void *context,uint32_t offset) {
    DollyAmigaImage *image=context;
    return Seek(image->file,(LONG)offset,OFFSET_BEGINNING)!=-1;
}
static int prepare_compression(DollyAmigaImage *image,DollyDlyCodec codec) {
    unsigned char *packed=malloc(image->dly.batch_bytes);
    uint16_t *hash=NULL;
    if(codec==DOLLY_DLY_LZSS)
        hash=malloc(DOLLY_DLY_HASH_SLOTS*sizeof(*hash));
    image->failure="Not enough memory for IFF DOLY compression.";
    if(packed && (codec!=DOLLY_DLY_LZSS || hash)) {
        image->failure="IFF DOLY image exceeds the single-file limit.";
        if(dolly_dly_compress(&image->dly,codec,packed,hash,seek_image))return 1;
    }
    free(hash);
    free(packed);
    return 0;
}
static void part_path(const DollyAmigaImage *image,uint32_t part,char path[528]) {
    if(part==1u)snprintf(path,528,"%s/%s.dly",image->partial,image->part_stem);
    else snprintf(path,528,"%s/%s.%03lu",image->partial,image->part_stem,(unsigned long)(part-1u));
}

/* Each destination owns a staging directory for a consecutive run of parts. */
static DollyImageDrawer *stage_split_drawer(const DollyAmigaImage *image,BPTR lock) {
    DollyImageDrawer *group=calloc(1,sizeof(*group));
    BPTR previous,staging=0;
    if(!group)return NULL;
    previous=CurrentDir(lock);
    if(unused_name(image->name))staging=CreateDir((STRPTR)image->partial);
    CurrentDir(previous);
    if(!staging) {
        free(group);
        return NULL;
    }
    UnLock(staging);
    group->lock=lock;
    return group;
}

static void use_split_drawer(DollyAmigaImage *image,DollyImageDrawer *group) {
    group->first=image->parts+1u;
    group->last=image->parts;
    group->previous=image->drawers;
    image->drawers=group;
    image->drawer=group->lock;
}

static int open_part(DollyAmigaImage *image) {
    DollySurfaceScan *reader=&image->reader;
    BPTR previous;
    uint64_t sectors=reader->total-reader->processed;
    image->part_start=reader->processed;
    image->part_opened=0;
    if(image->split) {
        if(sectors>image->part_sectors)sectors=image->part_sectors;
        part_path(image,image->parts+1u,image->staged_file);
    }
    image->part_end=reader->processed+sectors;
    if(image->format_dly) {
        free(image->dly.hash);
        free(image->dly.packed);
        image->failure="Cannot initialize the IFF DOLY part.";
        if(!dolly_dly_init(&image->dly,reader->first_lba+reader->processed,sectors,
                           reader->sector_bytes,reader->transfer_bytes) ||
           (image->codec && !prepare_compression(image,image->codec)))return 0;
    }
    previous=CurrentDir(image->drawer);
    image->file=Open((STRPTR)image->staged_file,MODE_NEWFILE);
    CurrentDir(previous);
    image->failure="Could not create the temporary image file.";
    if(!image->file)return 0;
    ++image->parts;
    image->part_opened=1;
    if(image->drawers)image->drawers->last=image->parts;
    if(image->format_dly) {
        int begun;
        if(image->split) {
            DollyDlySetInfo set={
                reader->first_lba,reader->total,reader->sector_bytes,
                (uint32_t)((reader->total-1u)/image->part_sectors+1u),image->part_sectors,
                image->parts,image->set_id
            };
            begun=dolly_dly_begin_set(&image->dly,write_image,image,&set);
        } else begun=dolly_dly_begin(&image->dly,write_image,image);
        if(!begun) {
            image->failure="Could not write the IFF DOLY header.";
            return 0;
        }
    }
    image->failure=NULL;
    return 1;
}

static int finish_part(DollyAmigaImage *image) {
    if(image->format_dly && !dolly_dly_finish(&image->dly)) {
        image->failure="IFF DOLY image is incomplete.";
        return 0;
    }
    if(!Close(image->file))image->failure="Could not close the temporary image.";
    image->file=0;
    return image->failure==NULL;
}

static void release_split(DollyAmigaImage *image,int commit) {
    DollyImageDrawer *group;
    int preserve=0;
    if(image->file)Close(image->file);
    image->file=0;
    if(commit)for(group=image->drawers;group;group=group->previous) {
        BPTR previous;
        if(group->last<group->first)continue;
        previous=CurrentDir(group->lock);
        group->published=unused_name(image->name) &&
                         Rename((STRPTR)image->partial,(STRPTR)image->name);
        CurrentDir(previous);
        if(!group->published) {
            DollyImageDrawer *rollback;
            image->failure="Could not publish all image parts. Check the destination drawers.";
            preserve=1;
            for(rollback=image->drawers;rollback!=group;rollback=rollback->previous) {
                if(!rollback->published)continue;
                previous=CurrentDir(rollback->lock);
                if(unused_name(image->partial) &&
                   Rename((STRPTR)image->name,(STRPTR)image->partial))rollback->published=0;
                CurrentDir(previous);
            }
            break;
        }
    }
    while((group=image->drawers)!=NULL) {
        BPTR previous=CurrentDir(group->lock);
        if(!commit) {
            char path[528];
            while(group->last>=group->first) {
                part_path(image,group->last--,path);
                DeleteFile((STRPTR)path);
            }
        }
        if(!preserve && !group->published &&
           !DeleteFile((STRPTR)image->partial))
            image->failure="Incomplete image remains in a .part drawer.";
        CurrentDir(previous);
        UnLock(group->lock);
        image->drawers=group->previous;
        free(group);
    }
    image->drawer=0;
}

static int discard_failed_part(DollyAmigaImage *image) {
    BPTR previous;
    int removed;
    if(image->file)Close(image->file);
    image->file=0;
    if(!image->part_opened)return 1;
    previous=CurrentDir(image->drawer);
    removed=DeleteFile((STRPTR)image->staged_file) || unused_name(image->staged_file);
    CurrentDir(previous);
    if(!removed)return 0;
    --image->parts;
    image->drawers->last=image->parts;
    image->part_opened=0;
    return 1;
}

static int retry_split_part(DollyAmigaImage *image) {
    DollySurfaceScan *reader=&image->reader;
    while(image->split && image->drawers && image->failure) {
        char path[256],message[250];
        BPTR drawer;
        DollyWarnings warnings={0};
        const char *problem;
        uint64_t sectors=image->part_end-image->part_start;
        uint64_t payload=sectors*reader->sector_bytes;
        uint64_t chunks=(payload-1u)/reader->transfer_bytes+1u;
        uint64_t required=payload+dolly_dly_overhead(chunks,1,image->codec!=DOLLY_DLY_RAW)+
                          DOLLY_DLY_NUMBERED_MSET_BYTES;
        snprintf(message,sizeof(message),"Part %lu: %s Choose a new destination to retry it from the beginning.",
                 (unsigned long)(image->parts+(image->part_opened?0u:1u)),image->failure);
        if(!dolly_amiga_role_request(image->parent,"Dolly - Image write failed",message,
                                    "Choose drawer...","Cancel",NULL) ||
           !dolly_amiga_pick_titled_drawer(image->parent,path,"Select new image destination"))return 0;
        drawer=Lock((STRPTR)path,ACCESS_READ);
        if(!drawer) {
            image->failure="Cannot open the selected drawer.";
            continue;
        }
        if(!dolly_amiga_read_guard_allows_lock(reader->mount_guard,drawer))
            problem="The selected drawer uses a source filesystem reserved for reading.";
        else
            problem=dolly_amiga_guard_file_transfer(&warnings,drawer,&reader->device,
                            reader->first_lba*reader->sector_bytes,reader->total*reader->sector_bytes);
        if(!problem && !check_file_destination(image,drawer,&warnings,payload,required,required,0))
            problem=image->failure;
        if(problem) {
            dolly_warnings_clear(&warnings);
            UnLock(drawer);
            image->failure=problem;
            continue;
        }
        if(!dolly_warnings_review(image->parent,&warnings)) {
            dolly_warnings_clear(&warnings);
            UnLock(drawer);
            image->failure="Part recovery cancelled.";
            return 0;
        }
        dolly_warnings_clear(&warnings);
        if(!dolly_amiga_read_guard_check(reader->mount_guard)) {
            UnLock(drawer);
            image->failure="Source filesystem changed while choosing a destination.";
            return 0;
        }
        {
            DollyImageDrawer *next=stage_split_drawer(image,drawer);
            if(!next) {
                UnLock(drawer);
                image->failure="Cannot stage a new image in this drawer. Check for an existing output.";
                continue;
            }
            if(!discard_failed_part(image)) {
                BPTR previous=CurrentDir(drawer);
                DeleteFile((STRPTR)image->partial);
                CurrentDir(previous);
                UnLock(drawer);
                free(next);
                image->failure="Cannot remove the incomplete part from the previous destination.";
                return 0;
            }
            use_split_drawer(image,next);
        }
        reader->processed=image->part_start;
        if(open_part(image))return 1;
    }
    return 0;
}

static void release(DollyAmigaImage *image,int commit) {
    if(image->drawers)release_split(image,commit);
    if(image->staging && !image->keep_staging) {
        BPTR previous=CurrentDir(image->drawer);
        if(image->file && !Close(image->file) && commit) {
            image->failure="Could not close the temporary image.";
            commit=0;
        }
        image->file=0;
        if(commit && (!unused_name(image->name) ||
                      !Rename((STRPTR)image->staged_file,
                              (STRPTR)image->name))) {
            image->failure="Could not publish image. Check destination.";
            commit=0;
        }
        if(!commit && !unused_name(image->staged_file))DeleteFile((STRPTR)image->staged_file);
        if(!DeleteFile((STRPTR)image->partial) && !commit)
            image->failure="Incomplete image remains in the .part drawer.";
        image->staging=0;
        CurrentDir(previous);
    }
    if(image->drawer) {
        UnLock(image->drawer);
        image->drawer=0;
    }
    dolly_surface_close(&image->reader);
    free(image->dly.hash);
    free(image->dly.packed);
    image->dly.hash=NULL;
    image->dly.packed=NULL;
}
void dolly_image_cancel(DollyAmigaImage *image) {
    release(image,0);
}
int dolly_image_open(DollyAmigaImage *image,const DollyEndpoint *source,
                     const DollyEndpoint *target,struct Window *parent) {
    BPTR previous,staging;
    const char *name;
    size_t prefix;
    char drawer[256];
    uint64_t bytes,encoded,single_bytes,split_parts=0,chunks,overhead;
    int needs_split;
    DollyWarnings warnings={0};
    dolly_image_cancel(image);
    memset(image,0,sizeof(*image));
    image->parent=parent;
    image->failure="Choose RAW, HDF or IFF DOLY output.";
    if(target->kind!=DOLLY_ENDPOINT_FILE ||
       (target->image_format!=DOLLY_IMAGE_RAW &&
        target->image_format!=DOLLY_IMAGE_HDF &&
        target->image_format!=DOLLY_IMAGE_IFF_DOLY &&
        target->image_format!=DOLLY_IMAGE_IFF_FAST &&
        target->image_format!=DOLLY_IMAGE_IFF_COMPACT))return 0;
    if(!dolly_surface_open(&image->reader,source,parent,DOLLY_IO_SOURCE,0,&warnings)) {
        image->failure=image->reader.failure;
        goto fail;
    }
    image->failure="Invalid image filename or destination drawer.";
    name=(const char *)FilePart((STRPTR)target->path);
    if(!name || !*name || !strcmp(name,".") || !strcmp(name,".."))goto fail;
    prefix=(size_t)(name-target->path);
    memcpy(drawer,target->path,prefix);
    drawer[prefix]='\0';
    image->drawer=Lock((STRPTR)drawer,ACCESS_READ);
    if(!image->drawer)goto fail;
    bytes=image->reader.total*image->reader.sector_bytes;
    image->format_dly=target->image_format!=DOLLY_IMAGE_RAW &&
                      target->image_format!=DOLLY_IMAGE_HDF;
    image->codec=target->image_format==DOLLY_IMAGE_IFF_FAST?DOLLY_DLY_PACKBITS:
                 target->image_format==DOLLY_IMAGE_IFF_COMPACT?DOLLY_DLY_LZSS:DOLLY_DLY_RAW;
    image->part_sectors=SPLIT_PART_BYTES/image->reader.transfer_bytes*
                       (image->reader.transfer_bytes/image->reader.sector_bytes);
    chunks=(bytes-1u)/image->reader.transfer_bytes+1u;
    overhead=image->format_dly?dolly_dly_overhead(chunks,1,image->codec!=DOLLY_DLY_RAW):0;
    single_bytes=overhead>UINT64_MAX-bytes?UINT64_MAX:bytes+overhead;
    needs_split=single_bytes>INT32_MAX ||
        (image->format_dly &&
         !dolly_dly_init(&image->dly,image->reader.first_lba,image->reader.total,
                         image->reader.sector_bytes,image->reader.transfer_bytes));
    encoded=single_bytes;
    if(needs_split) {
        split_parts=(image->reader.total-1u)/image->part_sectors+1u;
        if(split_parts>UINT32_MAX) {
            image->failure="Image requires too many parts.";
            goto fail;
        }
        overhead=dolly_dly_overhead(chunks,split_parts,image->codec!=DOLLY_DLY_RAW)+
                 split_parts*DOLLY_DLY_NUMBERED_MSET_BYTES;
        encoded=overhead>UINT64_MAX-bytes?UINT64_MAX:bytes+overhead;
    }
    if(!check_file_destination(image,image->drawer,&warnings,bytes,encoded,single_bytes,split_parts)) {
        dolly_amiga_role_request(parent,"Dolly - Create Image blocked",
                                 image->failure,"OK",NULL,NULL);
        goto fail;
    }
    image->failure=dolly_amiga_guard_file_transfer(&warnings,image->drawer,
                   &image->reader.device,
                   image->reader.first_lba*image->reader.sector_bytes,
                   image->reader.total*image->reader.sector_bytes);
    if(image->failure) {
        dolly_amiga_role_request(parent,"Dolly - Create Image blocked",
                                 image->failure,"OK",NULL,NULL);
        goto fail;
    }
    if(!dolly_amiga_guard_read(&image->reader.mount_guard,parent,&warnings,
                   &image->reader.device,image->reader.first_lba*image->reader.sector_bytes,
                   image->reader.total*image->reader.sector_bytes,image->drawer)) {
        image->failure="Cannot prepare source filesystem inhibition.";
        goto fail;
    }
    if(!dolly_warnings_review(parent,&warnings)) {
        image->failure="Operation cancelled.";
        goto fail;
    }
    dolly_warnings_clear(&warnings);
    if(!dolly_amiga_read_guard_check(image->reader.mount_guard)) {
        image->failure="Source filesystem changed before imaging.";
        goto fail;
    }

    if(image->split) {
        static uint16_t sequence;
        struct DateStamp stamp;
        const char *suffix=strrchr(name,'.');
        size_t stem=suffix && suffix!=name?(size_t)(suffix-name):strlen(name);
        DateStamp(&stamp);
        image->set_id=((((uint64_t)(uint32_t)stamp.ds_Days*1440u+
                       (uint32_t)stamp.ds_Minute)*3000u+(uint32_t)stamp.ds_Tick)<<16) |
                      ++sequence;
        if(!image->set_id)image->set_id=1;
        memcpy(image->part_stem,name,stem);
        image->part_stem[stem]='\0';
    }
    strcpy(image->name,name);
    strcpy(image->partial,name);
    strcat(image->partial,".part");
    image->failure="Destination exists or temporary drawer could not be created.";
    if(image->split) {
        DollyImageDrawer *group=stage_split_drawer(image,image->drawer);
        if(!group)goto fail;
        use_split_drawer(image,group);
    } else {
        previous=CurrentDir(image->drawer);
        if(unused_name(image->name) && (staging=CreateDir((STRPTR)image->partial))) {
            image->staging=1;
            UnLock(staging);
            snprintf(image->staged_file,sizeof(image->staged_file),"%s/image",image->partial);
        }
        CurrentDir(previous);
        if(!image->staging)goto fail;
    }
    if(!open_part(image) && !retry_split_part(image))goto fail;
    image->failure=NULL;
    return 1;
fail:
    dolly_warnings_clear(&warnings);
    release(image,0);
    return 0;
}
void dolly_image_step(DollyAmigaImage *image) {
    DollySurfaceScan *reader=&image->reader;
    uint64_t remaining,lba;
    size_t bytes;
    uint32_t sectors;
    DollyBlockReadResult result;
    if(!image->file || image->failure)return;
    remaining=image->part_end-reader->processed;
    lba=reader->first_lba+reader->processed;
    sectors=reader->transfer_bytes/reader->sector_bytes;
    if(!sectors)sectors=1;
    if(sectors>remaining)sectors=(uint32_t)remaining;
    bytes=(size_t)sectors*reader->sector_bytes;
    result=dolly_surface_read(reader,lba*reader->sector_bytes,
                            reader->buffer,bytes);
    if(result.status!=DOLLY_BLOCK_OK || result.bytes_read!=bytes) {
        char position[21];
        dolly_surface_log_decimal(position,lba);
        snprintf(image->read_failure,sizeof(image->read_failure),
                 "Read failed at LBA %s. Image incomplete.",position);
        image->failure=image->read_failure;
        return;
    }
    if(!(image->dly.file_bytes?
         dolly_dly_append(&image->dly,reader->buffer,bytes):
         write_image(image,reader->buffer,bytes))) {
        image->failure="Write failed. Check free space on destination.";
        if(image->split && !retry_split_part(image))release(image,0);
        return;
    }
    reader->processed+=sectors;
    if(reader->processed==image->part_end) {
        if(!finish_part(image)) {
            if(image->split) {
                if(!retry_split_part(image))release(image,0);
            } else if(!image->file)release(image,0);
        } else if(reader->processed==reader->total) {
            release(image,1);
        } else if(!open_part(image) && !retry_split_part(image)) {
            release(image,0);
        }
    }
}
