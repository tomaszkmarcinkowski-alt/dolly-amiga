#define _FILE_OFFSET_BITS 64
#define _POSIX_C_SOURCE 200809L
#include "dolly_partition_map.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define BS 512u
static void be32(unsigned char *p,unsigned n){p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}
static void le32(unsigned char *p,unsigned n){p[0]=n;p[1]=n>>8;p[2]=n>>16;p[3]=n>>24;}
static void sum(unsigned char *b,unsigned n){unsigned i,s=0;be32(b+4,n);be32(b+8,0);for(i=0;i<n;i++){unsigned j=i*4;s+=((unsigned)b[j]<<24)|((unsigned)b[j+1]<<16)|((unsigned)b[j+2]<<8)|b[j+3];}be32(b+8,0u-s);}
static void add_part(unsigned char *m,int slot,unsigned type,unsigned start,unsigned count){unsigned char *p=m+446+16*slot;p[4]=type;le32(p+8,start);le32(p+12,count);}
static void make_rdb(unsigned char *b){memset(b,0,BS);memcpy(b,"RDSK",4);be32(b+16,BS);be32(b+28,2);be32(b+64,100);be32(b+68,16);be32(b+72,4);sum(b,64);}
static void make_part(unsigned char *b,unsigned low,unsigned high){memset(b,0,BS);memcpy(b,"PART",4);be32(b+16,0xffffffffu);b[36]=3;memcpy(b+37,"DH0",3);be32(b+128,17);be32(b+132,128);be32(b+140,4);be32(b+144,1);be32(b+148,16);be32(b+164,low);be32(b+168,high);be32(b+192,0x444f5301);sum(b,64);}
static void store(FILE *f,unsigned long long offset,const unsigned char *b){assert(fseeko(f,(off_t)offset,SEEK_SET)==0);assert(fwrite(b,1,BS,f)==BS);assert(fflush(f)==0);}
static DollyMapStatus scan(const char *path,DollyPartitionMap *m){DollyBlockSource *s=NULL;DollyMapStatus v;assert(dolly_block_open_file(path,&s)==DOLLY_BLOCK_OK);v=dolly_partition_map_scan(s,512,m);dolly_block_close(s);return v;}
int main(void){char path[]="/tmp/dolly-mbr-XXXXXX";int fd=mkstemp(path);FILE *f;unsigned char mbr[BS]={0},rdb[BS],part[BS];DollyPartitionMap map;
 assert(fd>=0);f=fdopen(fd,"wb+");assert(f);
 assert(fseeko(f,(off_t)(12000u*BS-1),SEEK_SET)==0);assert(fputc(0,f)!=EOF);assert(fflush(f)==0);
 mbr[510]=0x55;mbr[511]=0xaa;add_part(mbr,0,0x0c,2048,2048);add_part(mbr,1,0x76,4096,4096);store(f,0,mbr);
 make_rdb(rdb);make_part(part,2,4);store(f,4096ULL*BS+BS,rdb);store(f,4096ULL*BS+2*BS,part);
 assert(scan(path,&map)==DOLLY_MAP_OK);assert(map.has_mbr);assert(map.partition_count==2);assert(map.primary[1].contains_rdb);assert(map.primary[1].nested_rdb.partition_count==1);
 assert(map.primary[1].nested_rdb.partitions[0].start_bytes==(4096ULL+2*64ULL)*BS);
 add_part(mbr,0,0x0c,4096,2048);store(f,0,mbr);assert(scan(path,&map)==DOLLY_MAP_OVERLAP);
 add_part(mbr,0,0x0c,2048,2048);
 make_part(part,60,70);store(f,4096ULL*BS+2*BS,part);store(f,0,mbr);
 assert(scan(path,&map)==DOLLY_MAP_OUT_OF_RANGE);
 make_part(part,2,4);store(f,4096ULL*BS+2*BS,part);
 part[40]^=0xff;store(f,4096ULL*BS+2*BS,part);assert(scan(path,&map)==DOLLY_MAP_NESTED_RDB_ERROR);
 make_part(part,2,4);store(f,4096ULL*BS+2*BS,part);
 add_part(mbr,0,0xee,1,1000);store(f,0,mbr);assert(scan(path,&map)==DOLLY_MAP_UNSUPPORTED);
 add_part(mbr,0,0x0c,10000,5000);store(f,0,mbr);assert(scan(path,&map)==DOLLY_MAP_OUT_OF_RANGE);
 /* Read-only whole-disk RDB coexisting with primary MBR. */
 add_part(mbr,0,0x0c,2048,2048);store(f,0,mbr);
 make_rdb(rdb);make_part(part,64,66);
 store(f,BS,rdb);store(f,2ULL*BS,part);
 assert(scan(path,&map)==DOLLY_MAP_OK);
 assert(map.has_mbr&&map.has_whole_disk_rdb&&map.hybrid_mbr_rdb);
 assert(map.hybrid_mbr_overlap);
 make_part(part,4,5);store(f,2ULL*BS,part);
 assert(scan(path,&map)==DOLLY_MAP_OK&&map.hybrid_mbr_rdb);
 assert(!map.hybrid_mbr_overlap);
 /* A corrupt whole-device RDB remains visible while MBR is valid. */
 rdb[40]^=1;store(f,BS,rdb);
 assert(scan(path,&map)==DOLLY_MAP_OK);
 assert(map.has_mbr && map.suspect_whole_disk_rdb && !map.has_whole_disk_rdb);
 assert(fclose(f)==0);assert(unlink(path)==0);
 puts("MBR/nested RDB test cases passed");return 0;}
