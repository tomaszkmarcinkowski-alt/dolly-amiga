#include "dolly_dly_codec.h"
#include <string.h>

static size_t run_length(const unsigned char *in,size_t size) {
    size_t length=1;
    while(length<size && length<128 && in[length]==in[0])++length;
    return length;
}
size_t dolly_dly_packbits(const unsigned char *in,size_t size,
                          unsigned char *out,size_t limit) {
    size_t pos=0,written=0;
    while(pos<size) {
        size_t length=run_length(in+pos,size-pos);
        if(length>=3) {
            if(limit-written<2)return 0;
            if(out) {
                out[written]=(unsigned char)(257-length);
                out[written+1]=in[pos];
            }
            written+=2;
            pos+=length;
        } else {
            size_t first=pos++;
            while(pos<size && pos-first<128) {
                if(size-pos>=3 && in[pos]==in[pos+1] && in[pos]==in[pos+2])break;
                ++pos;
            }
            length=pos-first;
            if(limit-written<=length)return 0;
            if(out) {
                out[written]=(unsigned char)(length-1);
                memcpy(out+written+1,in+first,length);
            }
            written+=length+1;
        }
    }
    return written;
}
static unsigned hash_at(const unsigned char *p) {
    return (((unsigned)p[0]<<8)^((unsigned)p[1]<<4)^p[2])&
           (DOLLY_DLY_HASH_SLOTS-1u);
}
size_t dolly_dly_lzss(const unsigned char *in,size_t size,
                     unsigned char *out,size_t limit,uint16_t *hash) {
    size_t pos=0,written=0;
    if(size>32768u)return 0;
    memset(hash,0,DOLLY_DLY_HASH_SLOTS*sizeof(*hash));
    while(pos<size) {
        unsigned bit;
        size_t control=written;
        if(written==limit)return 0;
        out[written++]=0;
        for(bit=0;bit<8 && pos<size;++bit) {
            size_t length=0,distance=0;
            if(size-pos>=3) {
                unsigned slot=hash_at(in+pos);
                unsigned previous=hash[slot];
                hash[slot]=(uint16_t)(pos+1);
                if(previous && pos-(previous-1u)<=4096u) {
                    size_t max=size-pos;
                    size_t match=previous-1u;
                    if(max>18)max=18;
                    while(length<max && in[match+length]==in[pos+length])++length;
                    distance=pos-match;
                }
            }
            if(length>=3) {
                unsigned pair=((unsigned)(distance-1)<<4)|(unsigned)(length-3);
                if(limit-written<2)return 0;
                out[control]|=(unsigned char)(1u<<bit);
                out[written++]=(unsigned char)(pair>>8);
                out[written++]=(unsigned char)pair;
                pos+=length;
            } else {
                if(written==limit)return 0;
                out[written++]=in[pos++];
            }
        }
    }
    return written;
}
int dolly_dly_decode(DollyDlyCodec codec,const unsigned char *in,size_t size,
                     unsigned char *out,size_t raw_size) {
    size_t pos=0,written=0;
    if(codec==DOLLY_DLY_RAW) {
        if(size!=raw_size)return 0;
        memcpy(out,in,size);
        return 1;
    }
    if(codec==DOLLY_DLY_PACKBITS) {
        while(pos<size && written<raw_size) {
            unsigned control=in[pos++];
            size_t count=control<128?control+1:257-control;
            if(control==128)continue;
            if(count>raw_size-written)return 0;
            if(control<128) {
                if(count>size-pos)return 0;
                memcpy(out+written,in+pos,count);
                pos+=count;
            } else {
                if(pos==size)return 0;
                memset(out+written,in[pos++],count);
            }
            written+=count;
        }
    } else if(codec==DOLLY_DLY_LZSS) {
        while(pos<size && written<raw_size) {
            unsigned bit,flags=in[pos++];
            for(bit=0;bit<8 && written<raw_size;++bit) {
                if(flags&(1u<<bit)) {
                    size_t distance,count;
                    unsigned pair;
                    if(size-pos<2)return 0;
                    pair=((unsigned)in[pos]<<8)|in[pos+1];
                    pos+=2;
                    distance=(pair>>4)+1;
                    count=(pair&15u)+3;
                    if(distance>written || count>raw_size-written)return 0;
                    while(count--) {
                        out[written]=out[written-distance];
                        ++written;
                    }
                } else {
                    if(pos==size)return 0;
                    out[written++]=in[pos++];
                }
            }
            if((flags>>bit)!=0)return 0;
        }
    } else return 0;
    return pos==size && written==raw_size;
}
