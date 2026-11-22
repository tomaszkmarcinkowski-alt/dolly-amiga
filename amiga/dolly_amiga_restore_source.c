#include "dolly_amiga_restore_source.h"
#include "dolly_dly_reader.h"
#include "dolly_amiga_endpoint_picker.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_physical_catalog.h"
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct ImageFile {
    BPTR file,lock;
} ImageFile;

static void close_image(void *opaque) {
    ImageFile *image=opaque;
    if(image->lock)UnLock(image->lock);
    if(image->file)Close(image->file);
    free(image);
}

static DollyBlockReadResult read_image(void *opaque,uint64_t offset,
                                      void *buffer,size_t bytes) {
    ImageFile *image=opaque;
    DollyBlockReadResult result={DOLLY_BLOCK_EIO,0};
    LONG actual;
    if(Seek(image->file,(LONG)offset,OFFSET_BEGINNING)==-1)return result;
    actual=Read(image->file,buffer,(LONG)bytes);
    if(actual<0)return result;
    result.bytes_read=(size_t)actual;
    result.status=result.bytes_read==bytes?DOLLY_BLOCK_OK:DOLLY_BLOCK_ESHORT;
    return result;
}

DollyBlockStatus dolly_amiga_image_file_size(BPTR file,uint64_t *size) {
    struct FileInfoBlock *info=AllocDosObject(DOS_FIB,NULL);
    DollyBlockStatus status=DOLLY_BLOCK_EIO;
    LONG reported=0,end,actual;
    unsigned char extra;
    if(!info)return DOLLY_BLOCK_ENOMEM;
    if(ExamineFH(file,info) && info->fib_DirEntryType<0) {
        reported=info->fib_Size;
        status=reported<0 || reported==INT32_MAX?
               DOLLY_BLOCK_EUNSUPPORTED:DOLLY_BLOCK_OK;
    }
    FreeDosObject(DOS_FIB,info);
    if(status!=DOLLY_BLOCK_OK)return status;
    /* A legacy handler may clamp or truncate its 32-bit size. Check
     * both reports and test EOF at the absolute offset before Restore
     * can treat the reported length as the entire image. */
    if(Seek(file,0,OFFSET_END)==-1)return DOLLY_BLOCK_EIO;
    end=Seek(file,0,OFFSET_BEGINNING);
    if(end<0 || end!=reported)return DOLLY_BLOCK_EUNSUPPORTED;
    if(Seek(file,end,OFFSET_BEGINNING)==-1)return DOLLY_BLOCK_EIO;
    actual=Read(file,&extra,1);
    if(actual<0)return DOLLY_BLOCK_EIO;
    if(actual!=0)return DOLLY_BLOCK_EUNSUPPORTED;
    if(Seek(file,0,OFFSET_BEGINNING)==-1)return DOLLY_BLOCK_EIO;
    *size=(uint64_t)end;
    return DOLLY_BLOCK_OK;
}

typedef struct ImageSetPart {
    ImageFile image;
    uint64_t bytes;
    const char *problem;
    int separate_volume;
} ImageSetPart;

typedef struct ImageSet {
    ImageFile *manifest;
    ImageSetPart *parts;
    DollyDlySetInfo info;
    DollyBlockSource *active;
    uint32_t active_index,failed_part;
} ImageSet;

static void keep_set_part(void *context) {
    (void)context;
}

DollyAppleMedia dolly_amiga_probe_apple_file(BPTR file,uint64_t size) {
    DollyBlockSource *blocks;
    ImageFile image={.file=file};
    DollyAppleMedia media;
    if(size%512u || size<1024u)return DOLLY_APPLE_UNKNOWN;
    if(dolly_block_open_backend(size,read_image,keep_set_part,&image,&blocks)!=DOLLY_BLOCK_OK)
        return DOLLY_APPLE_UNREADABLE;
    media=dolly_apple_media_probe(blocks,0,size);
    dolly_block_close(blocks);
    return media;
}

static void close_set(void *context) {
    ImageSet *set=context;
    dolly_block_close(set->active);
    if(set->parts) {
        uint32_t i;
        for(i=0;i<set->info.parts;++i) {
            if(set->parts[i].image.file)Close(set->parts[i].image.file);
            if(set->parts[i].image.lock)UnLock(set->parts[i].image.lock);
        }
    }
    close_image(set->manifest);
    free(set->parts);
    free(set);
}

static DollyBlockStatus activate_set_part(ImageSet *set,uint32_t index) {
    DollyBlockSource *container=NULL;
    ImageSetPart *part=&set->parts[index];
    DollyDlyImageInfo info;
    DollyBlockStatus status;
    if(set->active && set->active_index==index)return DOLLY_BLOCK_OK;
    dolly_block_close(set->active);
    set->active=NULL;
    status=dolly_block_open_backend(part->bytes,read_image,keep_set_part,&part->image,&container);
    if(status!=DOLLY_BLOCK_OK)return status;
    status=dolly_dly_open_lazy(container,&set->active,&info);
    if(status!=DOLLY_BLOCK_OK) {
        dolly_block_close(container);
        return status;
    }
    if(!dolly_dly_set_matches(&set->info,index,&info)) {
        dolly_block_close(set->active);
        set->active=NULL;
        return DOLLY_BLOCK_EINVAL;
    }
    set->active_index=index;
    return DOLLY_BLOCK_OK;
}

static DollyBlockReadResult read_set(void *context,uint64_t offset,void *data,size_t bytes) {
    ImageSet *set=context;
    const uint64_t part_bytes=(uint64_t)set->info.sectors_per_part*set->info.sector_bytes;
    DollyBlockReadResult result={DOLLY_BLOCK_OK,0};
    while(result.bytes_read<bytes) {
        uint64_t local=offset%part_bytes;
        size_t length=bytes-result.bytes_read;
        uint32_t index=(uint32_t)(offset/part_bytes);
        DollyBlockReadResult read;
        if(length>part_bytes-local)length=(size_t)(part_bytes-local);
        result.status=activate_set_part(set,index);
        if(result.status!=DOLLY_BLOCK_OK) {
            set->failed_part=index+1u;
            break;
        }
        read=dolly_block_read(set->active,local,(unsigned char *)data+result.bytes_read,length);
        result.bytes_read+=read.bytes_read;
        if(read.status!=DOLLY_BLOCK_OK) {
            result.status=read.status;
            set->failed_part=index+1u;
            break;
        }
        offset+=length;
    }
    return result;
}

static void set_member_name(char name[280],const char *stem,uint32_t index) {
    if(!stem)snprintf(name,280,"part%08lX.dly",(unsigned long)(index+1u));
    else if(!index)snprintf(name,280,"%s.dly",stem);
    else snprintf(name,280,"%s.%03lu",stem,(unsigned long)index);
}

static void set_stem(char stem[256],const char *path) {
    const char *name=(const char *)FilePart((STRPTR)path);
    const char *dot=strrchr(name,'.');
    size_t length=dot && dot!=name?(size_t)(dot-name):strlen(name);
    memcpy(stem,name,length);
    stem[length]='\0';
}

static const char *check_set_member(ImageSet *set,uint32_t index) {
    ImageSetPart *part=&set->parts[index];
    DollyBlockSource *container=NULL;
    DollyDlyImageInfo geometry;
    DollyBlockStatus status;
    if(!part->image.file)return "Missing or cannot open";
    if(dolly_amiga_image_file_size(part->image.file,&part->bytes)!=DOLLY_BLOCK_OK)
        return "Cannot measure file";
    if(set->info.part) {
        unsigned char header[DOLLY_DLY_GEOM_END+DOLLY_DLY_NUMBERED_MSET_BYTES];
        DollyDlySetInfo info;
        if(part->bytes<sizeof(header) ||
           read_image(&part->image,0,header,sizeof(header)).status!=DOLLY_BLOCK_OK ||
           dolly_dly_set_unpack(header,sizeof(header),&info)!=DOLLY_BLOCK_OK)
            return "Invalid part header";
        if(info.part!=index+1u || info.set_id!=set->info.set_id ||
           info.parts!=set->info.parts || info.first_lba!=set->info.first_lba ||
           info.sectors!=set->info.sectors || info.sector_bytes!=set->info.sector_bytes ||
           info.sectors_per_part!=set->info.sectors_per_part)
            return "Wrong part or another image";
    }
    if(dolly_block_open_backend(part->bytes,read_image,keep_set_part,&part->image,&container)
       !=DOLLY_BLOCK_OK)return "Cannot allocate part reader";
    status=dolly_dly_probe_header(container,&geometry);
    dolly_block_close(container);
    if(status!=DOLLY_BLOCK_OK || !dolly_dly_set_matches(&set->info,index,&geometry))
        return "Invalid IFF DOLY header or geometry";
    return NULL;
}

static int show_set_problems(ImageSet *set,const char *stem,struct Window *parent) {
    uint32_t i;
    char page[720];
    unsigned shown=0;
    size_t used=0;
    for(i=0;i<set->info.parts;++i) {
        char name[280];
        if(!set->parts[i].problem)continue;
        if(shown==4u) {
            if(!dolly_amiga_role_request(parent,"Dolly - Check image parts",page,"Next","Cancel",NULL))
                return 0;
            used=shown=0;
        }
        set_member_name(name,stem,i);
        used+=(size_t)snprintf(page+used,sizeof(page)-used,
                 "%lu/%lu: %.80s\n  %s\n",(unsigned long)(i+1u),
                 (unsigned long)set->info.parts,name,set->parts[i].problem);
        ++shown;
    }
    return !shown || dolly_amiga_role_request(parent,"Dolly - Check image parts",page,
                                              set->info.part?"Locate files...":"OK",
                                              set->info.part?"Cancel":NULL,NULL);
}

/* The set owns the manifest and holds the same file handles throughout
 * validation and transfer. One decoded index and cache are live at a time. */
static DollyBlockStatus open_set(DollyAmigaRestoreSource *source,ImageFile *manifest,
                                 uint64_t bytes,uint32_t sector_bytes,
                                 const DollyDlySetInfo *embedded,const char *path,
                                 struct Window *parent) {
    ImageSet *set=calloc(1,sizeof(*set));
    BPTR directory=0,previous;
    char text[256],name[280],stem[256];
    const char *numbered=NULL;
    DollyBlockStatus status=DOLLY_BLOCK_ENOMEM;
    uint32_t index;
    if(!set) {
        close_image(manifest);
        return status;
    }
    set->manifest=manifest;
    if(embedded) {
        set->info=*embedded;
        if(embedded->part) {
            set_stem(stem,path);
            numbered=stem;
        }
    } else {
        if(bytes>sizeof(text)) {
            status=DOLLY_BLOCK_EINVAL;
            goto done;
        }
        status=read_image(manifest,0,text,(size_t)bytes).status;
        if(status!=DOLLY_BLOCK_OK)goto done;
        status=dolly_dly_set_parse(text,(size_t)bytes,&set->info);
        if(status!=DOLLY_BLOCK_OK)goto done;
    }
    if(set->info.sector_bytes!=sector_bytes) {
        status=DOLLY_BLOCK_EINVAL;
        goto done;
    }
    status=DOLLY_BLOCK_EIO;
    manifest->lock=DupLockFromFH(manifest->file);
    directory=ParentOfFH(manifest->file);
    if(!manifest->lock || !directory)goto done;
    status=DOLLY_BLOCK_ENOMEM;
    if((uint64_t)set->info.parts*sizeof(*set->parts)>SIZE_MAX)goto done;
    set->parts=calloc(set->info.parts,sizeof(*set->parts));
    if(!set->parts)goto done;
    for(index=0;index<set->info.parts;++index) {
        ImageSetPart *part=&set->parts[index];
        if(embedded && !index) {
            part->image.file=manifest->file;
            manifest->file=0;
        } else {
            set_member_name(name,numbered,index);
            previous=CurrentDir(directory);
            part->image.file=Open((STRPTR)name,MODE_OLDFILE);
            CurrentDir(previous);
        }
        part->problem=check_set_member(set,index);
    }
    status=DOLLY_BLOCK_EIO;
    if(!show_set_problems(set,numbered,parent))goto done;
    for(index=0;index<set->info.parts;++index) {
        ImageSetPart *part=&set->parts[index];
        uint32_t prior;
        while(part->problem) {
            if(!set->info.part)goto done;
            DollyEndpoint replacement={0};
            char message[440];
            set_member_name(name,numbered,index);
            snprintf(message,sizeof(message),"Part %lu/%lu (%s): %s. Select a replacement file.",
                     (unsigned long)(index+1u),(unsigned long)set->info.parts,name,part->problem);
            if(!dolly_amiga_role_request(parent,"Dolly - Locate image part",message,
                                       "Browse...","Cancel",NULL) ||
               !dolly_amiga_pick_named_file(parent,&replacement,name))goto done;
            dolly_block_close(set->active);
            set->active=NULL;
            if(part->image.file)Close(part->image.file);
            part->image.file=Open((STRPTR)replacement.path,MODE_OLDFILE);
            part->problem=check_set_member(set,index);
        }
        part->image.lock=DupLockFromFH(part->image.file);
        if(!part->image.lock)goto done;
        part->separate_volume=SameLock(part->image.lock,manifest->lock)==LOCK_DIFFERENT;
        /* The manifest lock is checked by Restore, plus one per other volume. */
        for(prior=0;part->separate_volume && prior<index;++prior)
            if(set->parts[prior].separate_volume &&
               SameLock(set->parts[prior].image.lock,part->image.lock)!=LOCK_DIFFERENT)
                part->separate_volume=0;
    }
    status=dolly_block_open_backend(set->info.sectors*set->info.sector_bytes,
                                    read_set,close_set,set,&source->blocks);
    if(status==DOLLY_BLOCK_OK) {
        source->is_dly=1;
        source->set=set;
        source->lock=manifest->lock;
    }
done:
    if(directory)UnLock(directory);
    if(status!=DOLLY_BLOCK_OK)close_set(set);
    return status;
}

static DollyBlockStatus dolly_amiga_restore_source_open_first(DollyAmigaRestoreSource *source,
                         const DollyEndpoint *image,
                         uint32_t sector_bytes,struct Window *parent,uint64_t expected_set) {
    ImageFile *file;
    DollyBlockSource *container=NULL;
    unsigned char header[DOLLY_DLY_GEOM_END+DOLLY_DLY_NUMBERED_MSET_BYTES];
    const char *extension;
    int encoded;
    DollyBlockStatus status=DOLLY_BLOCK_EIO;
    uint64_t size=0;
    if(!source || source->blocks || !image || image->kind!=DOLLY_ENDPOINT_FILE ||
       !image->path[0])return DOLLY_BLOCK_EINVAL;
    if(image->image_format==DOLLY_IMAGE_ADF)return DOLLY_BLOCK_EUNSUPPORTED;
    /* Classic AmigaDOS Seek and ExamineFH use signed 32-bit positions. */
    file=calloc(1,sizeof(*file));
    if(!file)return DOLLY_BLOCK_ENOMEM;
    file->file=Open((STRPTR)image->path,MODE_OLDFILE);
    if(!file->file)goto fail;
    status=dolly_amiga_image_file_size(file->file,&size);
    if(status!=DOLLY_BLOCK_OK)goto fail;
    if(size>=12u) {
        size_t prefix=size<sizeof(header)?(size_t)size:sizeof(header);
        status=read_image(file,0,header,prefix).status;
        if(status!=DOLLY_BLOCK_OK)goto fail;
    }
    if(!expected_set && size>=sizeof(DOLLY_DLY_SET_MAGIC)-1u &&
       !memcmp(header,DOLLY_DLY_SET_MAGIC,sizeof(DOLLY_DLY_SET_MAGIC)-1u))
        return open_set(source,file,size,sector_bytes,NULL,image->path,parent);
    if(size>=DOLLY_DLY_GEOM_END+4u && !memcmp(header,"FORM",4) &&
       !memcmp(header+8,"DOLY",4) && !memcmp(header+DOLLY_DLY_GEOM_END,"MSET",4)) {
        DollyDlySetInfo set;
        size_t prefix=size<sizeof(header)?(size_t)size:sizeof(header);
        status=dolly_dly_set_unpack(header,prefix,&set);
        if(status!=DOLLY_BLOCK_OK)goto fail;
        if(expected_set && (set.set_id!=expected_set || set.part!=1u)) {
            status=DOLLY_BLOCK_EINVAL;
            goto fail;
        }
        if(set.part>1u) {
            DollyEndpoint first=*image;
            char stem[256],name[280],notice[420];
            BPTR first_lock;
            const char *filename=(const char *)FilePart((STRPTR)image->path);
            size_t directory_bytes=(size_t)(filename-image->path);
            set_stem(stem,image->path);
            set_member_name(name,stem,0);
            snprintf(notice,sizeof(notice),"You selected part %lu/%lu. Open %s, the first part?",
                     (unsigned long)set.part,(unsigned long)set.parts,name);
            status=DOLLY_BLOCK_EUNSUPPORTED;
            if(!dolly_amiga_role_request(parent,"Dolly - Image part",notice,"Open first","Cancel",NULL))
                goto fail;
            first_lock=0;
            if(directory_bytes+strlen(name)<sizeof(first.path)) {
                strcpy(first.path+directory_bytes,name);
                first_lock=Lock((STRPTR)first.path,ACCESS_READ);
            }
            if(first_lock)UnLock(first_lock);
            else if(!dolly_amiga_pick_named_file(parent,&first,name))goto fail;
            close_image(file);
            /* A renamed part must never trigger recursive redirection. */
            return dolly_amiga_restore_source_open_first(source,&first,sector_bytes,parent,set.set_id);
        }
        return open_set(source,file,size,sector_bytes,&set,image->path,parent);
    }
    if(expected_set) {status=DOLLY_BLOCK_EINVAL;goto fail;}
    encoded=size>=12u && !memcmp(header,"FORM",4) && !memcmp(header+8,"DOLY",4);
    if(!strcasecmp((const char *)FilePart((STRPTR)image->path),"manifest.txt")) {
        status=DOLLY_BLOCK_EINVAL;
        goto fail;
    }
    extension=strrchr(image->path,'.');
    if(!encoded) {
        if((extension && (!strcasecmp(extension,".dly") || !strcasecmp(extension,".doly"))) ||
           (image->image_format!=DOLLY_IMAGE_RAW && image->image_format!=DOLLY_IMAGE_HDF)) {
            status=DOLLY_BLOCK_EUNSUPPORTED;
            goto fail;
        }
        status=dolly_restore_check_raw(size,size,sector_bytes);
        if(status!=DOLLY_BLOCK_OK)goto fail;
    }
    file->lock=DupLockFromFH(file->file);
    status=dolly_block_open_backend(size,read_image,close_image,file,&container);
    if(status!=DOLLY_BLOCK_OK)goto fail;
    if(encoded) {
        DollyDlyImageInfo info;
        status=dolly_dly_open_lazy(container,&source->blocks,&info);
        if(status!=DOLLY_BLOCK_OK) {
            dolly_block_close(container);
            return status;
        }
        if(info.sector_bytes!=sector_bytes) {
            dolly_amiga_restore_source_close(source);
            return DOLLY_BLOCK_EINVAL;
        }
        source->is_dly=1;
    } else {
        source->blocks=container;
    }
    source->lock=file->lock;
    source->file=file->file;
    return DOLLY_BLOCK_OK;
fail:
    close_image(file);
    return status;
}

DollyBlockStatus dolly_amiga_restore_source_open(DollyAmigaRestoreSource *source,
                         const DollyEndpoint *image,uint32_t sector_bytes,struct Window *parent) {
    return dolly_amiga_restore_source_open_first(source,image,sector_bytes,parent,0);
}

const char *dolly_amiga_restore_build_apple(DollyAmigaRestoreSource *source,const char *template_path) {
    DollyBlockSource *template=NULL,*disk=NULL;
    uint64_t bytes;
    const char *error="Cannot open a flat Apple driver template.";
    if(!source->blocks || source->is_dly || source->apple_template)return error;
    ImageFile *file=calloc(1,sizeof(*file));
    if(!file)return "Not enough memory for the Apple driver template.";
    file->file=Open((STRPTR)template_path,MODE_OLDFILE);
    if(!file->file || dolly_amiga_image_file_size(file->file,&bytes)!=DOLLY_BLOCK_OK)goto failed;
    file->lock=DupLockFromFH(file->file);
    if(dolly_block_open_backend(bytes,read_image,close_image,file,&template)!=DOLLY_BLOCK_OK)goto failed;
    error=dolly_apple_disk_from_filedisk(source->blocks,template,&disk);
    if(error)goto failed;
    source->apple_volume=source->blocks;
    source->apple_template=template;
    source->blocks=disk;
    source->template_lock=file->lock;
    source->template_file=file->file;
    return NULL;
failed:
    if(template)dolly_block_close(template);
    else close_image(file);
    return error;
}

BPTR dolly_amiga_restore_volume(const DollyAmigaRestoreSource *source,uint32_t index) {
    uint32_t i;
    if(!index)return source->lock;
    if(source->set)for(i=0;i<source->set->info.parts;++i)
        if(source->set->parts[i].separate_volume && !--index)
            return source->set->parts[i].image.lock;
    return 0;
}

int dolly_amiga_restore_uses_mount(const DollyAmigaRestoreSource *source,
                                 const DollyMountedVolume *mount) {
    const ImageSet *set=source->set;
    if(!mount->handler)return 0;
    if(dolly_amiga_file_uses_mount(source->file,mount) ||
       dolly_amiga_lock_uses_mount(source->lock,mount) ||
       dolly_amiga_file_uses_mount(source->template_file,mount) ||
       dolly_amiga_lock_uses_mount(source->template_lock,mount))return 1;
    if(!set)return 0;
    if(dolly_amiga_file_uses_mount(set->manifest->file,mount))return 1;
    for(uint32_t i=0;i<set->info.parts;++i) {
        const ImageFile *part=&set->parts[i].image;
        if(dolly_amiga_file_uses_mount(part->file,mount) ||
           dolly_amiga_lock_uses_mount(part->lock,mount))return 1;
    }
    return 0;
}

uint32_t dolly_amiga_restore_failed_part(const DollyAmigaRestoreSource *source,uint32_t *total) {
    *total=source->set?source->set->info.parts:0;
    return source->set?source->set->failed_part:0;
}

void dolly_amiga_restore_source_close(DollyAmigaRestoreSource *source) {
    dolly_block_close(source->blocks);
    dolly_block_close(source->apple_volume);
    dolly_block_close(source->apple_template);
    memset(source,0,sizeof(*source));
}
