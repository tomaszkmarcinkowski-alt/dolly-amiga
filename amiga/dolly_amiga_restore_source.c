#include "dolly_amiga_restore_source.h"
#include <proto/dos.h>
#include <stdlib.h>

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

DollyBlockStatus dolly_amiga_restore_source_open(DollyAmigaRestoreSource *source,
                         const DollyEndpoint *image,
                         uint32_t sector_bytes) {
    struct FileInfoBlock *info;
    ImageFile *file;
    DollyBlockStatus status=DOLLY_BLOCK_EIO;
    uint64_t size=0;
    if(!source || source->blocks || !image || image->kind!=DOLLY_ENDPOINT_FILE ||
       !image->path[0])return DOLLY_BLOCK_EINVAL;
    if(image->image_format!=DOLLY_IMAGE_RAW && image->image_format!=DOLLY_IMAGE_HDF)
        return DOLLY_BLOCK_EUNSUPPORTED;
    /* Classic AmigaDOS Seek and ExamineFH use signed 32-bit positions. */
    file=calloc(1,sizeof(*file));
    if(!file)return DOLLY_BLOCK_ENOMEM;
    file->file=Open((STRPTR)image->path,MODE_OLDFILE);
    if(!file->file)goto fail;
    info=AllocDosObject(DOS_FIB,NULL);
    if(!info) {
        status=DOLLY_BLOCK_ENOMEM;
        goto fail;
    }
    if(ExamineFH(file->file,info) && info->fib_DirEntryType<0) {
        if(info->fib_Size>=0) {
            size=(uint64_t)info->fib_Size;
            status=dolly_restore_check_raw(size,size,sector_bytes);
        } else status=DOLLY_BLOCK_EUNSUPPORTED;
    }
    FreeDosObject(DOS_FIB,info);
    if(status!=DOLLY_BLOCK_OK)goto fail;
    file->lock=DupLockFromFH(file->file);
    status=dolly_block_open_backend(size,read_image,close_image,file,&source->blocks);
    if(status!=DOLLY_BLOCK_OK)goto fail;
    source->lock=file->lock;
    return DOLLY_BLOCK_OK;
fail:
    close_image(file);
    return status;
}

void dolly_amiga_restore_source_close(DollyAmigaRestoreSource *source) {
    dolly_block_close(source->blocks);
    source->blocks=NULL;
    source->lock=0;
}
