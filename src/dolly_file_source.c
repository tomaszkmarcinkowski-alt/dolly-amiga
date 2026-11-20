#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "dolly_block_source.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef _WIN32
typedef int64_t DollyFileOffset;
#define file_seek _fseeki64
#define file_tell _ftelli64
#else
typedef off_t DollyFileOffset;
#define file_seek fseeko
#define file_tell ftello
#endif
static DollyBlockReadResult file_read(void *ctx,uint64_t off,void *dst,size_t len){
    DollyBlockReadResult r={DOLLY_BLOCK_EIO,0};
    FILE *f=(FILE *)ctx;
    if(off>INT64_MAX || (uint64_t)(DollyFileOffset)off!=off){
        r.status=DOLLY_BLOCK_EUNSUPPORTED;return r;
    }
    clearerr(f);
    if(file_seek(f,(DollyFileOffset)off,SEEK_SET)!=0)return r;
    r.bytes_read=fread(dst,1,len,f);
    r.status=(r.bytes_read==len && !ferror(f))?DOLLY_BLOCK_OK:DOLLY_BLOCK_EIO;
    return r;
}
static void file_close(void *ctx){fclose((FILE *)ctx);}
DollyBlockStatus dolly_block_open_file(const char *path,DollyBlockSource **out){
    FILE *f;DollyFileOffset end;DollyBlockStatus result;
    if(!out || !path || !*path)return DOLLY_BLOCK_EINVAL;
    *out=NULL;
    f=fopen(path,"rb");if(!f)return DOLLY_BLOCK_EIO;
    if(file_seek(f,0,SEEK_END)!=0 || (end=file_tell(f))<0 || file_seek(f,0,SEEK_SET)!=0){
        fclose(f);return DOLLY_BLOCK_EIO;
    }
    result=dolly_block_open_backend((uint64_t)end,file_read,file_close,f,out);
    if(result!=DOLLY_BLOCK_OK)fclose(f);
    return result;
}
