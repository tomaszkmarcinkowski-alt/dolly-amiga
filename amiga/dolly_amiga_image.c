#include "dolly_amiga_image.h"
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static void release(DollyAmigaImage *image,int commit) {
    if(image->staging) {
        BPTR previous=CurrentDir(image->drawer);
        if(image->file && !Close(image->file) && commit) {
            image->failure="Could not close the temporary image.";
            commit=0;
        }
        image->file=0;
        if(commit && (!unused_name(image->name) ||
                      !Rename((STRPTR)image->staged_file,(STRPTR)image->name))) {
            image->failure="Could not publish image. Check destination.";
            commit=0;
        }
        if(!commit && !unused_name(image->staged_file))
            DeleteFile((STRPTR)image->staged_file);
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
    DollyPhysicalEntry destination={0};
    BPTR previous,staging;
    const char *name;
    size_t prefix;
    char drawer[256];
    uint32_t batch_bytes;
    int destination_kind;
    dolly_image_cancel(image);
    memset(image,0,sizeof(*image));
    image->failure="Choose RAW, HDF or IFF DOLY output.";
    if(target->kind!=DOLLY_ENDPOINT_FILE ||
       (target->image_format!=DOLLY_IMAGE_RAW &&
        target->image_format!=DOLLY_IMAGE_HDF &&
        target->image_format!=DOLLY_IMAGE_IFF_DOLY &&
        target->image_format!=DOLLY_IMAGE_IFF_FAST &&
        target->image_format!=DOLLY_IMAGE_IFF_COMPACT))return 0;
    if(!dolly_surface_open(&image->reader,source,parent)) {
        image->failure=image->reader.failure;
        return 0;
    }
    image->failure="Image must be smaller than 2 GiB on this writer.";
    if(image->reader.total*image->reader.sector_bytes>INT32_MAX)goto fail;
    if(target->image_format==DOLLY_IMAGE_IFF_DOLY ||
       target->image_format==DOLLY_IMAGE_IFF_FAST ||
       target->image_format==DOLLY_IMAGE_IFF_COMPACT) {
        batch_bytes=image->reader.transfer_bytes;
        image->failure="IFF DOLY image exceeds the single-file limit.";
        if(!dolly_dly_init(&image->dly,image->reader.first_lba,image->reader.total,
                          image->reader.sector_bytes,batch_bytes))goto fail;
        if(target->image_format!=DOLLY_IMAGE_IFF_DOLY &&
           !prepare_compression(image,target->image_format==DOLLY_IMAGE_IFF_FAST?
                                DOLLY_DLY_PACKBITS:DOLLY_DLY_LZSS))goto fail;
    }
    image->failure="Invalid image filename or destination drawer.";
    name=(const char *)FilePart((STRPTR)target->path);
    if(!name || !*name || !strcmp(name,".") || !strcmp(name,".."))goto fail;
    prefix=(size_t)(name-target->path);
    memcpy(drawer,target->path,prefix);
    drawer[prefix]='\0';
    image->drawer=Lock((STRPTR)drawer,ACCESS_READ);
    if(!image->drawer)goto fail;
    image->failure="Choose RAM: or a verified different device.";
    destination_kind=dolly_physical_from_lock(image->drawer,&destination);
    if(destination_kind!=2 && (destination_kind!=1 ||
       strcmp(destination.driver,image->reader.device.driver)!=0 ||
       destination.unit==image->reader.device.unit))goto fail;

    strcpy(image->name,name);
    strcpy(image->partial,name);
    strcat(image->partial,".part");
    previous=CurrentDir(image->drawer);
    if(unused_name(image->name) && (staging=CreateDir((STRPTR)image->partial))) {
        image->staging=1;
        UnLock(staging);
        strcpy(image->staged_file,image->partial);
        strcat(image->staged_file,"/image");
        image->file=Open((STRPTR)image->staged_file,MODE_NEWFILE);
    }
    CurrentDir(previous);
    image->failure="Destination exists or temporary file could not be created.";
    if(!image->file)goto fail;
    if(image->dly.file_bytes && !dolly_dly_begin(&image->dly,write_image,image)) {
        image->failure="Could not write the IFF DOLY header.";
        goto fail;
    }
    image->failure=NULL;
    return 1;
fail:
    release(image,0);
    return 0;
}
void dolly_image_step(DollyAmigaImage *image) {
    DollySurfaceScan *reader=&image->reader;
    uint64_t remaining,lba;
    size_t bytes;
    uint32_t sectors;
    DollyBlockReadResult result;
    if(!image->file)return;
    remaining=reader->total-reader->processed;
    lba=reader->first_lba+reader->processed;
    sectors=reader->transfer_bytes/reader->sector_bytes;
    if(!sectors)sectors=1;
    if(sectors>remaining)sectors=(uint32_t)remaining;
    bytes=(size_t)sectors*reader->sector_bytes;
    result=dolly_block_read(reader->source,lba*reader->sector_bytes,
                            reader->buffer,bytes);
    if(result.status!=DOLLY_BLOCK_OK || result.bytes_read!=bytes) {
        char position[21];
        dolly_surface_log_decimal(position,lba);
        printf("CREATE IMAGE: read failed at LBA %s (%s)\n",position,
               dolly_block_status_name(result.status));
        image->failure="Read failed. Image was not completed.";
        release(image,0);
        return;
    }
    if(!(image->dly.file_bytes?
         dolly_dly_append(&image->dly,reader->buffer,bytes):
         write_image(image,reader->buffer,bytes))) {
        image->failure="Write failed. Check free space on destination.";
        release(image,0);
        return;
    }
    reader->processed+=sectors;
    if(reader->processed==reader->total) {
        if(image->dly.file_bytes && !dolly_dly_finish(&image->dly))
            image->failure="IFF DOLY image is incomplete.";
        release(image,image->failure==NULL);
    }
}
