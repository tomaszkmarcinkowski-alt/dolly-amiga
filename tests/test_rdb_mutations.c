#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "dolly_partition_map.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define B 512u
#define TOTAL (256u * B)
#define PSTART (16u * B)
#define FIXTURE_SIZE TOTAL
static uint32_t seed=0xD011A600u;
static uint32_t next_rand(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void be32(unsigned char *p,uint32_t x){p[0]=(unsigned char)(x>>24);p[1]=(unsigned char)(x>>16);p[2]=(unsigned char)(x>>8);p[3]=(unsigned char)x;}
static void le32(unsigned char *p,uint32_t x){p[0]=(unsigned char)x;p[1]=(unsigned char)(x>>8);p[2]=(unsigned char)(x>>16);p[3]=(unsigned char)(x>>24);}
static void finish(unsigned char *p,unsigned words){unsigned i;uint32_t total=0;be32(p+4,words);be32(p+8,0);for(i=0;i<words;i++)total+=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];be32(p+8,0u-total);}
static void base_image(unsigned char *p){
 unsigned char *r,*part;
 memset(p,0,TOTAL);
 p[510]=0x55;p[511]=0xaa;
 p[446+4]=0x76;le32(p+446+8,16);le32(p+446+12,128);
 r=p+PSTART+B;
 memcpy(r,"RDSK",4);be32(r+16,B);be32(r+28,2);be32(r+64,10);be32(r+68,16);be32(r+72,4);finish(r,64);
 part=p+PSTART+2*B;
 memcpy(part,"PART",4);be32(part+16,UINT32_MAX);part[36]=3;memcpy(part+37,"DH0",3);
 be32(part+128,17);be32(part+132,128);be32(part+140,4);be32(part+144,1);be32(part+148,16);
 be32(part+164,1);be32(part+168,1);be32(part+192,0x444f5301);finish(part,64);
}
int main(void){
 unsigned char *base=(unsigned char *)malloc(TOTAL),*mut=(unsigned char *)malloc(TOTAL);
 DollyPartitionMap *map=(DollyPartitionMap *)malloc(sizeof(*map));
 char path[]="/tmp/dolly-mutate-XXXXXX";int fd;
 FILE *file;unsigned i,j;
 assert(base&&mut&&map);
 base_image(base);fd=mkstemp(path);assert(fd>=0);file=fdopen(fd,"wb+");assert(file);
 for(i=0;i<600;i++){
  DollyBlockSource *src=NULL;
  DollyMapStatus st;
  unsigned n=1u+(next_rand()%6u);
  memcpy(mut,base,TOTAL);
  for(j=0;j<n;j++){
   uint32_t r=next_rand();size_t at;
   switch(r%4u){
    case 0:at=(size_t)(r>>4)%512;break;
    case 1:at=PSTART+B+((r>>4)%512u);break;
    case 2:at=PSTART+2*B+((r>>4)%512u);break;
    default:at=(size_t)(r>>5)%TOTAL;break;
   }
   mut[at]^=(unsigned char)(1u<<(r%8u));
  }
  assert(fseeko(file,0,SEEK_SET)==0);
  assert(fwrite(mut,1,TOTAL,file)==TOTAL);
  assert(fflush(file)==0);
  assert(dolly_block_open_file(path,&src)==DOLLY_BLOCK_OK);
  st=dolly_partition_map_scan(src,512,map);
  assert(st>=DOLLY_MAP_OK && st<=DOLLY_MAP_NESTED_RDB_ERROR);
  dolly_block_close(src);
 }
 assert(fclose(file)==0);assert(unlink(path)==0);
 free(map);free(mut);free(base);
 puts("600 deterministic damaged MBR/RDB cases passed without crashing");return 0;
}
