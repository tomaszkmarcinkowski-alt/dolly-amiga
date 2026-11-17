#define _POSIX_C_SOURCE 200809L
#include "dolly_block_source.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
struct DollyBlockSource { FILE *file; uint64_t length; };
const char *dolly_block_status_name(DollyBlockStatus s) {
 switch(s) {
 case DOLLY_BLOCK_OK:return "OK";
 case DOLLY_BLOCK_EINVAL:return "Invalid argument";
 case DOLLY_BLOCK_ERANGE:return "Range exceeds source";
 case DOLLY_BLOCK_ESHORT:return "Short read";
 case DOLLY_BLOCK_EIO:return "File I/O error";
 case DOLLY_BLOCK_ENOMEM:return "Out of memory";
 case DOLLY_BLOCK_EUNSUPPORTED:return "Unsupported host offset";
 default:return "Unknown error";
 }
}
DollyBlockStatus dolly_block_open_file(const char *path,DollyBlockSource **out) {
 FILE *file; off_t end; DollyBlockSource *src;
 if (!out || !path || !*path) return DOLLY_BLOCK_EINVAL;
 *out=NULL;
 file=fopen(path,"rb"); if(!file) return DOLLY_BLOCK_EIO;
 if(fseeko(file,0,SEEK_END)!=0 || (end=ftello(file))<0 ||
    fseeko(file,0,SEEK_SET)!=0) {fclose(file);return DOLLY_BLOCK_EIO;}
 src=(DollyBlockSource*)malloc(sizeof(*src));
 if(!src){fclose(file);return DOLLY_BLOCK_ENOMEM;}
 src->file=file;src->length=(uint64_t)end;*out=src;
 return DOLLY_BLOCK_OK;
}
uint64_t dolly_block_size(const DollyBlockSource *src) {
 return src?src->length:0;
}
DollyBlockReadResult dolly_block_read(DollyBlockSource *src,
 uint64_t offset,void *buffer,size_t bytes) {
 DollyBlockReadResult result={DOLLY_BLOCK_EINVAL,0};
 uint64_t available;size_t want;
 if(!src||(!buffer&&bytes!=0))return result;
 if(offset>src->length){result.status=DOLLY_BLOCK_ERANGE;return result;}
 if(bytes==0){result.status=DOLLY_BLOCK_OK;return result;}
 if(offset>INT64_MAX||(uint64_t)(off_t)offset!=offset){
  result.status=DOLLY_BLOCK_EUNSUPPORTED;return result;
 }
 if(fseeko(src->file,(off_t)offset,SEEK_SET)!=0){
  result.status=DOLLY_BLOCK_EIO;return result;
 }
 available=src->length-offset; want=bytes;
 if(available<(uint64_t)want)want=(size_t)available;
 result.bytes_read=fread(buffer,1,want,src->file);
 if(result.bytes_read!=want||ferror(src->file)){
  result.status=DOLLY_BLOCK_EIO;return result;
 }
 result.status=want==bytes?DOLLY_BLOCK_OK:DOLLY_BLOCK_ESHORT;
 return result;
}
void dolly_block_close(DollyBlockSource *src) {
 if(src){if(src->file)fclose(src->file);free(src);}
}
