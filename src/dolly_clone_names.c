#include "dolly_clone_names.h"
#include <string.h>

static uint32_t read_long(const unsigned char *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}

static uint32_t sum_longs(const unsigned char *block,size_t bytes) {
    uint32_t sum=0;
    size_t offset;
    for(offset=0;offset<bytes;offset+=4)sum+=read_long(block+offset);
    return sum;
}

static int valid_name(const unsigned char *name,size_t size) {
    size_t i;
    for(i=0;i<size;++i)
        if(name[i]<32 || name[i]==127 || name[i]==':' || name[i]=='/')return 0;
    return size!=0;
}

static size_t name_fields(DollyCloneNameKind kind,const unsigned char *block,size_t bytes,
                          size_t *checked,size_t *maximum,size_t *checksum) {
    if(bytes<512 || bytes>32768 || (bytes&(bytes-1)))return 0;
    if(kind==DOLLY_CLONE_PART_NAME) {
        uint32_t words=read_long(block+4);
        uint32_t env=read_long(block+128);
        if(bytes!=512 || memcmp(block,"PART",4) || words<17 || words>128 ||
           env<11 || env>95 || sum_longs(block,(33u+env)*4u))return 0;
        *checked=words*4u;
        *maximum=31;
        *checksum=8;
        return 36;
    }
    if(kind!=DOLLY_CLONE_VOLUME_NAME || read_long(block)!=2 ||
       read_long(block+bytes-4)!=1 || read_long(block+bytes-200)!=UINT32_MAX)return 0;
    *checked=bytes;
    *maximum=30;
    *checksum=20;
    return bytes-80;
}

int dolly_clone_read_name(DollyCloneNameKind kind,const void *data,size_t bytes,char name[32]) {
    const unsigned char *block=data;
    size_t checked,maximum,checksum,offset;
    if(!data || !name)return 0;
    offset=name_fields(kind,block,bytes,&checked,&maximum,&checksum);
    if(!offset || sum_longs(block,checked) ||
       block[offset]>maximum || !valid_name(block+offset+1,block[offset]))return 0;
    memcpy(name,block+offset+1,block[offset]);
    name[block[offset]]=0;
    return 1;
}

int dolly_clone_rename(DollyCloneNameKind kind,void *data,size_t bytes,
                       const char *old_name,const char *new_name) {
    unsigned char *block=data;
    char current[32];
    size_t checked,maximum,checksum,offset,length;
    uint32_t seal;
    if(!old_name || !new_name || !dolly_clone_read_name(kind,data,bytes,current) ||
       strcmp(current,old_name))return 0;
    offset=name_fields(kind,block,bytes,&checked,&maximum,&checksum);
    length=strlen(new_name);
    if(length>maximum || !valid_name((const unsigned char *)new_name,length))return 0;
    memset(block+offset,0,maximum+1u);
    block[offset]=(unsigned char)length;
    memcpy(block+offset+1,new_name,length);
    memset(block+checksum,0,4);
    seal=0u-sum_longs(block,checked);
    block[checksum]=(unsigned char)(seal>>24);
    block[checksum+1]=(unsigned char)(seal>>16);
    block[checksum+2]=(unsigned char)(seal>>8);
    block[checksum+3]=(unsigned char)seal;
    return 1;
}
