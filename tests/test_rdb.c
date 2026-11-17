#define _POSIX_C_SOURCE 200809L
#include "dolly_rdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define BS 512u
static void wr32(unsigned char *p,unsigned n){p[0]=(unsigned char)(n>>24);p[1]=(unsigned char)(n>>16);p[2]=(unsigned char)(n>>8);p[3]=(unsigned char)n;}
static void finish_sum(unsigned char *b,unsigned longs){unsigned i;unsigned s=0;wr32(b+4,longs);wr32(b+8,0);for(i=0;i<longs;i++){unsigned j=i*4;s+=((unsigned)b[j]<<24)|((unsigned)b[j+1]<<16)|((unsigned)b[j+2]<<8)|b[j+3];}wr32(b+8,0u-s);}
static void make_rdb(unsigned char *p,unsigned next){memset(p,0,BS);memcpy(p,"RDSK",4);wr32(p+16,BS);wr32(p+28,next);wr32(p+64,100);wr32(p+68,16);wr32(p+72,4);finish_sum(p,64);}
static void make_part(unsigned char *p,unsigned next){memset(p,0,BS);memcpy(p,"PART",4);wr32(p+16,next);p[36]=3;memcpy(p+37,"DH0",3);wr32(p+128,17);wr32(p+132,128);wr32(p+140,4);wr32(p+148,16);wr32(p+164,2);wr32(p+168,5);wr32(p+192,0x444f5301);finish_sum(p,64);}
static void store(FILE *f,unsigned long long where,const unsigned char *b){assert(fseeko(f,(off_t)where,SEEK_SET)==0);assert(fwrite(b,1,BS,f)==BS);}
static DollyRdbStatus run(const char *path,unsigned long long base,DollyRdb *r){DollyBlockSource *src=NULL;DollyRdbStatus status;assert(dolly_block_open_file(path,&src)==DOLLY_BLOCK_OK);status=dolly_rdb_scan(src,base,BS,16,r);dolly_block_close(src);return status;}
int main(void){char path[]="/tmp/dolly-rdb-XXXXXX";int fd=mkstemp(path);FILE *f;unsigned char rdb[BS],part[BS];DollyRdb r;unsigned long long base=2048ULL*BS;assert(fd>=0);f=fdopen(fd,"wb+");assert(f);make_rdb(rdb,2);make_part(part,DOLLY_RDB_END);store(f,base+BS,rdb);store(f,base+2*BS,part);assert(fseeko(f,(off_t)(base+500ULL*BS-1),SEEK_SET)==0);assert(fputc(0,f)==0);fflush(f);
 assert(run(path,base,&r)==DOLLY_RDB_OK);assert(r.header_block==1);assert(r.partition_count==1);assert(strcmp(r.partitions[0].name,"DH0")==0);assert(r.partitions[0].start_bytes==base+2ULL*4*16*BS);assert(r.partitions[0].length_bytes==4ULL*4*16*BS);
 assert(run(path,0,&r)==DOLLY_RDB_NOT_FOUND);
 /* 256 longwords per filesystem block = 1024 bytes, RDB device blocks 512. */
 make_part(part,DOLLY_RDB_END);wr32(part+132,256);finish_sum(part,64);
 store(f,base+2*BS,part);
 assert(fseeko(f,(off_t)(base+1600ULL*BS-1),SEEK_SET)==0);
 assert(fputc(0,f)==0);fflush(f);
 assert(run(path,base,&r)==DOLLY_RDB_OK);
 assert(r.partitions[0].start_bytes==base+2ULL*4*16*1024u);
 assert(r.partitions[0].length_bytes==4ULL*4*16*1024u);
 make_part(part,DOLLY_RDB_END);store(f,base+2*BS,part);fflush(f);

 part[40]^=1;store(f,base+2*BS,part);fflush(f);assert(run(path,base,&r)==DOLLY_RDB_BAD_CHECKSUM);
 make_part(part,2);store(f,base+2*BS,part);fflush(f);assert(run(path,base,&r)==DOLLY_RDB_CYCLE);
 make_part(part,DOLLY_RDB_END);wr32(part+168,300);finish_sum(part,64);store(f,base+2*BS,part);fflush(f);assert(run(path,base,&r)==DOLLY_RDB_OUT_OF_RANGE);
 make_part(part,DOLLY_RDB_END);store(f,base+2*BS,part);rdb[40]^=1;store(f,base+BS,rdb);fflush(f);assert(run(path,base,&r)==DOLLY_RDB_BAD_CHECKSUM);
 /* Valid secondary RDSK takes priority over damaged earlier candidate. */
 make_rdb(rdb,2);store(f,base+3*BS,rdb);fflush(f);
 assert(run(path,base,&r)==DOLLY_RDB_OK&&r.header_block==3);
 assert(fclose(f)==0);assert(unlink(path)==0);puts("RDB tests passed");return 0;}
