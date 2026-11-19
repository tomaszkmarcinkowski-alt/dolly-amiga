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

DollyBlockStatus dolly_amiga_image_file_size(BPTR file,uint64_t *size) {
    struct FileInfoBlock *info=AllocDosObject(DOS_FIB,NULL);
    DollyBlockStatus status=DOLLY_BLOCK_EIO;
    LONG end;
    if(!info)return DOLLY_BLOCK_ENOMEM;
    if(ExamineFH(file,info) && info->fib_DirEntryType<0)
        status=info->fib_Size<0?DOLLY_BLOCK_EUNSUPPORTED:DOLLY_BLOCK_OK;
    FreeDosObject(DOS_FIB,info);
    if(status!=DOLLY_BLOCK_OK)return status;
    /* Seek returns the previous position. The second call returns EOF
     * and rewinds the handle for subsequent reads. */
    if(Seek(file,0,OFFSET_END)==-1)return DOLLY_BLOCK_EIO;
    end=Seek(file,0,OFFSET_BEGINNING);
    if(end<0)return DOLLY_BLOCK_EUNSUPPORTED;
    *size=(uint64_t)end;
    return DOLLY_BLOCK_OK;
}

DollyBlockStatus dolly_amiga_restore_source_open(DollyAmigaRestoreSource *source,
                         const DollyEndpoint *image,
                         uint32_t sector_bytes) {
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
    status=dolly_amiga_image_file_size(file->file,&size);
    if(status!=DOLLY_BLOCK_OK)goto fail;
    status=dolly_restore_check_raw(size,size,sector_bytes);
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
