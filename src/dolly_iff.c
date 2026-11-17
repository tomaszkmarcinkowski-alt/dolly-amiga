#include "dolly_iff.h"
#include <string.h>
#include <stdint.h>
#define IFF_MAX_SIGNED_SIZE UINT32_C(0x7fffffff)
#define IFF_MAX_CHUNKS UINT32_C(1000000)
static uint32_t be32(const unsigned char *p){
 return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];
}
static DollyIffStatus get(DollyBlockSource *source,uint64_t offset,
                          unsigned char *data,size_t bytes){
 DollyBlockReadResult r=dolly_block_read(source,offset,data,bytes);
 if(r.status==DOLLY_BLOCK_OK && r.bytes_read==bytes)return DOLLY_IFF_OK;
 if(r.status==DOLLY_BLOCK_ESHORT||r.status==DOLLY_BLOCK_ERANGE)return DOLLY_IFF_TRUNCATED;
 return DOLLY_IFF_IO_ERROR;
}
DollyIffStatus dolly_iff_scan(DollyBlockSource *source,
                             DollyIffVisitor visitor,void *ctx,
                             DollyIffSummary *out){
 unsigned char hdr[12];uint64_t pos,end,total;
 uint32_t form_size;DollyIffStatus status;
 if(!source||!out)return DOLLY_IFF_BAD_ARGUMENT;
 memset(out,0,sizeof(*out));total=dolly_block_size(source);
 out->total_file_bytes=total;
 if(total<12)return DOLLY_IFF_TRUNCATED;
 status=get(source,0,hdr,12);if(status!=DOLLY_IFF_OK)return status;
 if(memcmp(hdr,"FORM",4)!=0 || memcmp(hdr+8,"DOLY",4)!=0)
  return DOLLY_IFF_BAD_FORM;
 form_size=be32(hdr+4);
 if(form_size<4 || form_size>IFF_MAX_SIGNED_SIZE)return DOLLY_IFF_BAD_SIZE;
 end=(uint64_t)form_size+8;
 if(end!=total)return DOLLY_IFF_BAD_SIZE;
 out->form_data_bytes=form_size;
 pos=12;
 while(pos<end){
  DollyIffChunk ch;uint64_t available,padded;
  unsigned char cb[8];uint32_t n;
  if(out->chunk_count>=IFF_MAX_CHUNKS)return DOLLY_IFF_TOO_MANY;
  available=end-pos;
  if(available<8)return DOLLY_IFF_BAD_CHUNK;
  status=get(source,pos,cb,8);if(status!=DOLLY_IFF_OK)return status;
  n=be32(cb+4);
  if(n>IFF_MAX_SIGNED_SIZE)return DOLLY_IFF_BAD_SIZE;
  padded=(uint64_t)n+(n&1u);
  if(padded>available-8)return DOLLY_IFF_BAD_CHUNK;
  memcpy(ch.id,cb,4);ch.id[4]='\0';
  ch.header_offset=pos;ch.data_offset=pos+8;ch.data_size=n;
  out->chunk_count++;
  if(visitor && visitor(ctx,&ch))return DOLLY_IFF_STOPPED;
  pos+=8+padded;
 }
 return pos==end?DOLLY_IFF_OK:DOLLY_IFF_BAD_CHUNK;
}
const char *dolly_iff_status_name(DollyIffStatus s){
 switch(s){
 case DOLLY_IFF_OK:return "OK";
 case DOLLY_IFF_BAD_ARGUMENT:return "Bad arguments";
 case DOLLY_IFF_TRUNCATED:return "Truncated input";
 case DOLLY_IFF_BAD_FORM:return "Not FORM DOLY";
 case DOLLY_IFF_BAD_SIZE:return "Invalid IFF length";
 case DOLLY_IFF_BAD_CHUNK:return "Chunk exceeds FORM boundary";
 case DOLLY_IFF_TOO_MANY:return "Too many chunks";
 case DOLLY_IFF_IO_ERROR:return "I/O error";
 case DOLLY_IFF_STOPPED:return "Visitor stopped scan";
 default:return "Unknown IFF error";
 }
}
