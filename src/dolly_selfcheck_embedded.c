#include "dolly_selfcheck.h"
#include "dolly_checksum.h"
#include <string.h>
/* This *referenced* global is intentionally present in the on-disk program.
 * The packager edits its 8 ASCII CRC digits and 16-byte release field
 * AFTER linking. Keep exactly one instance in the file.
 * Do not mark static or strip this object from the final executable. */
/* Explicit byte initializers: exactly 40 bytes, no NUL terminator.
 * A C string literal would contain 41 bytes and trigger -Werror. */
const unsigned char dolly_selfcheck_record[DOLLY_SELF_RECORD_SIZE] = {
    'D','O','L','L','Y',':','C','R','C','3','2',':','V','1','!','!',
    '0','0','0','0','0','0','0','0',
    'D','O','L','L','Y','-','U','N','S','T','A','M','P','E','D','!'
};

static int hex_value(unsigned char ch){
    if(ch>='0'&&ch<='9')return (int)(ch-'0');
    if(ch>='A'&&ch<='F')return (int)(ch-'A'+10);
    return -1;
}
static int parse_record(const unsigned char *record,uint32_t *crc){
    unsigned i,release_nonblank=0;
    uint32_t val=0;
    if(!record||memcmp(record,dolly_selfcheck_record,
                        DOLLY_SELF_MAGIC_SIZE)!=0)return 0;
    for(i=0;i<DOLLY_SELF_CRC_SIZE;i++){
        int digit=hex_value(record[DOLLY_SELF_CRC_OFFSET+i]);
        if(digit<0)return 0;
        val=(val<<4)|(uint32_t)digit;
    }
    if(val==0)return -1; /* reserved unpatched value */
    for(i=0;i<DOLLY_SELF_RELEASE_SIZE;i++){
        unsigned char ch=record[DOLLY_SELF_RELEASE_OFFSET+i];
        if(ch<32||ch>126)return 0;
        if(ch!=' ')release_nonblank=1;
    }
    if(!release_nonblank ||
       memcmp(record+DOLLY_SELF_RELEASE_OFFSET,
              "DOLLY-UNSTAMPED!",DOLLY_SELF_RELEASE_SIZE)==0)return -1;
    *crc=val;return 1;
}
DollySelfCheckReport dolly_selfcheck_embedded_stream(
    DollySelfRead reader,void *ctx,
    const unsigned char loaded_record[DOLLY_SELF_RECORD_SIZE]){
    DollySelfCheckReport result={0};
    DollyChecksum sum;
    unsigned char chunk[512],pending[DOLLY_SELF_MAGIC_SIZE];
    size_t queued=0;
    unsigned field=0,mode=0,found=0;
    uint32_t embedded_crc=0;
    int valid=parse_record(loaded_record,&embedded_crc);
    if(!reader || !loaded_record || valid!=1){
        result.result=valid<0?DOLLY_SELF_RECORD_UNSTAMPED:
                              DOLLY_SELF_RECORD_INVALID;
        return result;
    }
    dolly_checksum_init(&sum);
    for(;;){
        long n=reader(ctx,chunk,sizeof(chunk));
        size_t i;
        if(n<0 || (unsigned long)n>sizeof(chunk)){
            result.result=DOLLY_SELF_READ_FAILED;return result;
        }
        if(n==0)break;
        if(result.actual_size>UINT64_MAX-(uint64_t)n){
            result.result=DOLLY_SELF_READ_FAILED;return result;
        }
        result.actual_size+=(uint64_t)n;
        for(i=0;i<(size_t)n;i++){
            unsigned char byte=chunk[i];
            if(mode==1){ /* The eight CRC digits are treated as ASCII zeroes. */
                if(byte!=loaded_record[DOLLY_SELF_CRC_OFFSET+field] ||
                   hex_value(byte)<0){
                    result.result=DOLLY_SELF_RECORD_INVALID;return result;
                }
                if(dolly_checksum_update(&sum,"0",1)!=0){
                    result.result=DOLLY_SELF_READ_FAILED;return result;
                }
                if(++field==DOLLY_SELF_CRC_SIZE){mode=2;field=0;}
            }else if(mode==2){ /* Ensure disk record matches loaded record. */
                if(byte!=loaded_record[DOLLY_SELF_RELEASE_OFFSET+field]){
                    result.result=DOLLY_SELF_RECORD_INVALID;return result;
                }
                if(dolly_checksum_update(&sum,&byte,1)!=0){
                    result.result=DOLLY_SELF_READ_FAILED;return result;
                }
                if(++field==DOLLY_SELF_RELEASE_SIZE){mode=0;field=0;}
            }else{
                pending[queued++]=byte;
                if(queued==DOLLY_SELF_MAGIC_SIZE){
                    if(memcmp(pending,loaded_record,DOLLY_SELF_MAGIC_SIZE)==0){
                        if(++found!=1){
                            result.result=DOLLY_SELF_RECORD_INVALID;return result;
                        }
                        if(dolly_checksum_update(&sum,pending,queued)!=0){
                            result.result=DOLLY_SELF_READ_FAILED;return result;
                        }
                        queued=0;mode=1;field=0;
                    }else{
                        if(dolly_checksum_update(&sum,pending,1)!=0){
                            result.result=DOLLY_SELF_READ_FAILED;return result;
                        }
                        memmove(pending,pending+1,--queued);
                    }
                }
            }
        }
    }
    if(mode!=0){
        result.result=DOLLY_SELF_RECORD_INVALID;return result;
    }
    if(dolly_checksum_update(&sum,pending,queued)!=0){
        result.result=DOLLY_SELF_READ_FAILED;return result;
    }
    if(found!=1){
        result.result=DOLLY_SELF_RECORD_MISSING;return result;
    }
    result.actual_crc32=dolly_checksum_final(&sum);
    result.result=(result.actual_crc32==embedded_crc)?
                   DOLLY_SELF_OK:DOLLY_SELF_CRC_MISMATCH;
    return result;
}
