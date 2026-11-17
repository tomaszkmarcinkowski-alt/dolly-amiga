#define _POSIX_C_SOURCE 200809L
#include "dolly_iff.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void wr32(unsigned char *p,uint32_t v){p[0]=v>>24;p[1]=v>>16;p[2]=v>>8;p[3]=v;}
static DollyIffStatus probe(const unsigned char *p,size_t n,unsigned *chunks){
 char name[]="/tmp/dolly-iff-XXXXXX";int fd=mkstemp(name);FILE *f;
 DollyBlockSource *src=NULL;DollyIffSummary sum;DollyIffStatus status;
 assert(fd>=0);f=fdopen(fd,"wb");assert(f);
 assert(fwrite(p,1,n,f)==n);assert(fclose(f)==0);
 assert(dolly_block_open_file(name,&src)==DOLLY_BLOCK_OK);
 status=dolly_iff_scan(src,NULL,NULL,&sum);*chunks=sum.chunk_count;
 dolly_block_close(src);assert(unlink(name)==0);return status;
}
int main(void){
 unsigned char p[128]={0};unsigned chunks;unsigned i;
 memcpy(p,"FORM",4);wr32(p+4,4);memcpy(p+8,"DOLY",4);
 assert(probe(p,12,&chunks)==DOLLY_IFF_OK&&chunks==0);
 memcpy(p+12,"ANNO",4);wr32(p+16,3);memcpy(p+20,"abc",3);p[23]=0xfe;
 wr32(p+4,16);assert(probe(p,24,&chunks)==DOLLY_IFF_OK&&chunks==1);
 assert(probe(p,23,&chunks)==DOLLY_IFF_BAD_SIZE);
 wr32(p+16,4);assert(probe(p,24,&chunks)==DOLLY_IFF_OK);
 wr32(p+16,5);assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_CHUNK);
 wr32(p+16,0xffffffffu);assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_SIZE);
 wr32(p+16,0);wr32(p+4,16);
 assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_CHUNK);
 wr32(p+4,0xffffffffu);assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_SIZE);
 wr32(p+4,16);p[8]='X';assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_FORM);p[8]='D';
 p[0]='X';assert(probe(p,24,&chunks)==DOLLY_IFF_BAD_FORM);p[0]='F';
 assert(probe(p,5,&chunks)==DOLLY_IFF_TRUNCATED);
 for(i=0;i<256;i++){
  p[4]=(unsigned char)i;p[5]=(unsigned char)(i*3);
  {DollyIffStatus s=probe(p,24,&chunks);assert(s!=DOLLY_IFF_IO_ERROR);}
 }
 puts("IFF structural tests passed");return 0;
}
