#include "dolly_block_target.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct Fixture {
    FILE *file;
    uint64_t file_base;
    unsigned calls;
    int short_write,flush_error,short_read,corrupt;
} Fixture;

static DollyBlockStatus write_blocks(void *context,uint64_t offset,
                                    const void *data,size_t bytes,size_t *written) {
    Fixture *f=context;
    assert(f->calls++==0);
    assert(offset>=f->file_base && offset-f->file_base<=2048);
    assert(fseek(f->file,(long)(offset-f->file_base),SEEK_SET)==0);
    *written=fwrite(data,1,bytes-(size_t)f->short_write,f->file);
    return DOLLY_BLOCK_OK;
}
static DollyBlockStatus flush_blocks(void *context) {
    Fixture *f=context;
    assert(f->calls++==1);
    return !f->flush_error && fflush(f->file)==0?DOLLY_BLOCK_OK:DOLLY_BLOCK_EIO;
}
static DollyBlockReadResult read_blocks(void *context,uint64_t offset,void *data,
                                       size_t bytes) {
    Fixture *f=context;
    DollyBlockReadResult result={DOLLY_BLOCK_OK,0};
    assert(f->calls++==2);
    assert(offset>=f->file_base && offset-f->file_base<=2048);
    assert(fseek(f->file,(long)(offset-f->file_base),SEEK_SET)==0);
    result.bytes_read=fread(data,1,bytes-(size_t)f->short_read,f->file);
    if(f->corrupt)((unsigned char *)data)[0]^=1;
    return result;
}

int main(void) {
    Fixture f={0};
    DollyBlockTarget target={2048,512,1024,512,512,&f,write_blocks,flush_blocks,read_blocks,0};
    unsigned char image[2048],data[1024],scratch[1024];
    size_t i;
    f.file=tmpfile();
    assert(f.file);
    memset(image,0x5a,sizeof(image));
    memset(data,0xa5,sizeof(data));
    assert(fwrite(image,1,sizeof(image),f.file)==sizeof(image));
    assert(dolly_block_write_verified(&target,512,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_VERIFIED);
    assert(f.calls==3 && fseek(f.file,0,SEEK_SET)==0);
    assert(fread(image,1,sizeof(image),f.file)==sizeof(image));
    for(i=0;i<sizeof(image);++i)
        assert(image[i]==(i>=1024 && i<1536?0xa5:0x5a));
    f.calls=0;
    assert(dolly_block_write_verified(&target,1024,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_REJECTED);
    assert(dolly_block_write_verified(&target,1,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_REJECTED);
    assert(dolly_block_write_verified(&target,0,data,1024,scratch,sizeof(scratch))
           ==DOLLY_WRITE_REJECTED);
    assert(dolly_block_write_verified(&target,0,data,512,data+1,512)
           ==DOLLY_WRITE_REJECTED);
    assert(f.calls==0);
    target.capacity=UINT64_C(0x100000200);
    target.start=UINT64_C(0xfffffe00);
    assert(dolly_block_write_verified(&target,0,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_REJECTED && f.calls==0);
    target.length=512;
    f.file_base=target.start;
    assert(dolly_block_write_verified(&target,0,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_VERIFIED && f.calls==3);
    f.calls=0;
    f.file_base=0;
    target.start=512;
    target.length=1024;
    f.short_write=1;
    assert(dolly_block_write_verified(&target,0,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_FAILED && f.calls==1);
    f.calls=0;
    f.short_write=0;
    f.flush_error=1;
    assert(dolly_block_write_verified(&target,0,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_FLUSH_FAILED && f.calls==2);
    f.calls=0;
    f.flush_error=0;
    f.short_read=1;
    assert(dolly_block_write_verified(&target,0,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_VERIFY_FAILED && f.calls==3);
    f.calls=0;
    f.short_read=0;
    f.corrupt=1;
    assert(dolly_block_write_verified(&target,0,data,512,scratch,sizeof(scratch))
           ==DOLLY_WRITE_MISMATCH && f.calls==3);
    f.calls=0;
    assert(dolly_block_write(&target,0,data,512,0,NULL,0)
           ==DOLLY_WRITE_COMPLETE && f.calls==2);
    f.calls=0;
    target.readback=NULL;
    assert(dolly_block_write(&target,0,data,512,1,scratch,sizeof(scratch))
           ==DOLLY_WRITE_REJECTED && f.calls==0);
    assert(dolly_block_write(&target,0,data,512,0,NULL,0)
           ==DOLLY_WRITE_COMPLETE && f.calls==2);
    f.calls=0;
    f.flush_error=1;
    assert(dolly_block_write(&target,0,data,512,0,NULL,0)
           ==DOLLY_WRITE_FLUSH_FAILED && f.calls==2);
    assert(fclose(f.file)==0);
    return 0;
}
