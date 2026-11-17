#include "dolly_amiga_source.h"
#include "amiga_mock_exec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
int main(void){
 DollyBlockSource *s=NULL;DollyBlockReadResult r;
 DollyAmigaDeviceOptions opt={
     "mock.device",0,UINT64_C(0x200000000),512,1024,
     0x00fffffeu,2u,1u,1u
 };
 unsigned char buf[2048];unsigned i;
 /* Fail closed before opening anything if DMA limits are unknown. */
 opt.dma_profile_verified=0;
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EINVAL && s==NULL);
 opt.dma_profile_verified=1;
 opt.dma_alignment=3;
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EINVAL && s==NULL);
 opt.dma_alignment=2;
 dolly_mock_set_dma_address(UINT64_C(0x01000000));
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EUNSUPPORTED && s==NULL);
 dolly_mock_set_dma_address(UINT64_C(0x00100000));
 dolly_mock_set_nsd(0);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 assert((dolly_mock_last_alloc_flags() & (1u<<9))!=0);
 r=dolly_block_read(s,512,buf,2048);assert(r.status==DOLLY_BLOCK_OK&&r.bytes_read==2048);
 for(i=0;i<2048;i++)assert(buf[i]==(unsigned char)((512+i)&255));
 r=dolly_block_read(s,UINT64_C(0x100000000),buf,512);
 assert(r.status==DOLLY_BLOCK_EUNSUPPORTED&&r.bytes_read==0);
 assert(!dolly_mock_last_read64());dolly_block_close(s);
 dolly_mock_set_nsd(1);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 r=dolly_block_read(s,UINT64_C(0x100000200),buf,1024);
 assert(r.status==DOLLY_BLOCK_OK&&r.bytes_read==1024&&dolly_mock_last_read64());
 for(i=0;i<1024;i++)assert(buf[i]==(unsigned char)((512+i)&255));
 r=dolly_block_read(s,UINT64_C(0xfffffe00),buf,1024);
 assert(r.status==DOLLY_BLOCK_OK&&dolly_mock_last_read64());
 r=dolly_block_read(s,513,buf,512);assert(r.status==DOLLY_BLOCK_EINVAL);
 dolly_block_close(s);
 puts("Mock Exec backend checks passed");return 0;
}
