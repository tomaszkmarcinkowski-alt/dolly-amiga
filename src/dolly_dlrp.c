#include "dolly_dlrp.h"
#include <string.h>

void dolly_dlrp_u32(unsigned char *out,uint32_t value) {
    out[0]=(unsigned char)(value>>24);out[1]=(unsigned char)(value>>16);
    out[2]=(unsigned char)(value>>8);out[3]=(unsigned char)value;
}
void dolly_dlrp_u64(unsigned char *out,uint64_t value) {
    dolly_dlrp_u32(out,(uint32_t)(value>>32));
    dolly_dlrp_u32(out+4,(uint32_t)value);
}
int dolly_dlrp_form(DollyDlySink sink,void *ctx,uint32_t body_bytes) {
    unsigned char header[12]="FORM";
    if(body_bytes>UINT32_MAX-4u || body_bytes%2u)return 0;
    dolly_dlrp_u32(header+4,body_bytes+4u);
    memcpy(header+8,"DLRP",4);
    return sink(ctx,header,sizeof(header));
}
int dolly_dlrp_chunk(DollyDlySink sink,void *ctx,const char id[4],
                     const void *data,uint32_t bytes) {
    unsigned char header[8],padding=0;
    memcpy(header,id,4);
    dolly_dlrp_u32(header+4,bytes);
    return sink(ctx,header,sizeof(header)) && (!bytes || sink(ctx,data,bytes)) &&
           (!(bytes&1u) || sink(ctx,&padding,1));
}
