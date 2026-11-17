#define _POSIX_C_SOURCE 200809L
#include "dolly_block_source.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){
 char name[]="/tmp/dolly-block-XXXXXX";
 int fd=mkstemp(name); FILE *f;
 DollyBlockSource *src=NULL; DollyBlockReadResult r;
 unsigned char buf[16];const unsigned char data[]={0x11,0x22,0x33,0x44,0x55};
 assert(fd>=0);f=fdopen(fd,"wb");assert(f);
 assert(fwrite(data,1,sizeof(data),f)==sizeof(data));assert(fclose(f)==0);
 assert(dolly_block_open_file(name,&src)==DOLLY_BLOCK_OK && src);
 assert(dolly_block_size(src)==sizeof(data));
 r=dolly_block_read(src,1,buf,3);
 assert(r.status==DOLLY_BLOCK_OK&&r.bytes_read==3);
 assert(memcmp(buf,data+1,3)==0);
 r=dolly_block_read(src,4,buf,4);
 assert(r.status==DOLLY_BLOCK_ESHORT&&r.bytes_read==1&&buf[0]==0x55);
 r=dolly_block_read(src,5,buf,1);
 assert(r.status==DOLLY_BLOCK_ESHORT&&r.bytes_read==0);
 r=dolly_block_read(src,UINT64_MAX,buf,1);
 assert(r.status==DOLLY_BLOCK_ERANGE&&r.bytes_read==0);
 r=dolly_block_read(src,0,NULL,1);assert(r.status==DOLLY_BLOCK_EINVAL);
 r=dolly_block_read(src,5,NULL,0);assert(r.status==DOLLY_BLOCK_OK);
 r=dolly_block_read(src,6,buf,0);assert(r.status==DOLLY_BLOCK_ERANGE);
 dolly_block_close(src);src=NULL;
 f=fopen(name,"wb+");assert(f);
 assert(fseeko(f,(off_t)UINT64_C(4294967300),SEEK_SET)==0);
 assert(fputc(0x7b,f)==0x7b);assert(fclose(f)==0);
 assert(dolly_block_open_file(name,&src)==DOLLY_BLOCK_OK);
 assert(dolly_block_size(src)==UINT64_C(4294967301));
 r=dolly_block_read(src,UINT64_C(4294967300),buf,1);
 assert(r.status==DOLLY_BLOCK_OK&&r.bytes_read==1&&buf[0]==0x7b);
 dolly_block_close(src);assert(unlink(name)==0);
 puts("BlockSource tests passed");return 0;
}
