#include "dolly_amiga_source.h"
#include "amiga_mock_exec.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static unsigned dma_report_count;
static DollyDmaStatus dma_last_reason;
static void report_dma(DollyDmaStatus reason,const char *message,void *user){
 unsigned *count=(unsigned *)user;
 assert(message&&strlen(message)>20);
 assert(strstr(message,"stopped")!=NULL || strstr(message,"failed")!=NULL);
 ++*count;
 dma_last_reason=reason;
}

int main(void){
 DollyBlockSource *s=NULL;DollyBlockReadResult r;
 DollyAmigaDeviceOptions opt={
     "mock.device",0,UINT64_C(0x200000000),512,1024,
     0x00fffffeu,2u,1u,1u,report_dma,&dma_report_count,0,0,0,0,0
 };
 unsigned char buf[2048];unsigned i;
 struct IOStdReq request={0};
 dolly_mock_set_nsd(3);
 assert(dolly_amiga_device_advertises_write(&request));
 dolly_mock_set_nsd(4);
 assert(!dolly_amiga_device_advertises_write(&request));
 assert(request.io_Data==NULL);
 /* Fail closed before opening anything if DMA limits are unknown. */
 opt.dma_profile_verified=0;
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EINVAL && s==NULL);
 assert(dma_report_count==1&&dma_last_reason==DOLLY_DMA_BAD_PROFILE);
 opt.dma_profile_verified=1;
 opt.dma_alignment=3;
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EINVAL && s==NULL);
 assert(dma_report_count==2&&dma_last_reason==DOLLY_DMA_BAD_PROFILE);
 opt.dma_alignment=2;
 dolly_mock_set_dma_address(UINT64_C(0x01000000));
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EUNSUPPORTED && s==NULL);
 assert(dma_report_count==3&&dma_last_reason==DOLLY_DMA_OUTSIDE_MASK);
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
 /* A successful NSD query without READ64 in SupportedCommands is NOT
  * permission to issue the 64-bit command. */
 dolly_mock_set_nsd(2);
 dolly_mock_read_fault(0,0);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 r=dolly_block_read(s,UINT64_C(0x100000000),buf,512);
 assert(r.status==DOLLY_BLOCK_EUNSUPPORTED && r.bytes_read==0);
 dolly_block_close(s);
 /* A short second transfer must not be reported as successful. */
 dolly_mock_set_nsd(1);
 dolly_mock_read_fault(2,1);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 r=dolly_block_read(s,0,buf,2048);
 assert(r.status==DOLLY_BLOCK_ESHORT && r.bytes_read==1024);
 dolly_block_close(s);
 /* A real I/O error must preserve completed byte count only. */
 dolly_mock_read_fault(2,2);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 r=dolly_block_read(s,0,buf,2048);
 assert(r.status==DOLLY_BLOCK_EIO && r.bytes_read==1024);
 dolly_block_close(s);
 dolly_mock_read_fault(0,0);
 opt.required_read_end=UINT64_C(0x100000800);
 dolly_mock_set_nsd(0);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EUNSUPPORTED && s==NULL);
 opt.verified_uae_ide=1;
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 r=dolly_block_read(s,UINT64_C(0xfffffe00),buf,1024);
 assert(r.status==DOLLY_BLOCK_OK && r.bytes_read==1024);
 assert(dolly_mock_last_scsi_lba()==0x7fffffu && buf[0]==0xa5 && buf[1023]==0xa5);
 r=dolly_block_read(s,UINT64_C(0x100000200),buf,512);
 assert(r.status==DOLLY_BLOCK_OK && r.bytes_read==512);
 assert(dolly_mock_last_scsi_lba()==0x800001u);
 dolly_mock_read_fault(1,1);
 r=dolly_block_read(s,UINT64_C(0x100000200),buf,512);
 assert(r.status==DOLLY_BLOCK_ESHORT && r.bytes_read==0);
 dolly_mock_read_fault(1,3);
 r=dolly_block_read(s,UINT64_C(0x100000200),buf,512);
 assert(r.status==DOLLY_BLOCK_EUNSUPPORTED && r.bytes_read==0);
 dolly_mock_read_fault(1,4);
 r=dolly_block_read(s,UINT64_C(0x100000200),buf,512);
 assert(r.status==DOLLY_BLOCK_EIO && r.bytes_read==0);
 dolly_mock_read_fault(0,0);
 dolly_block_close(s);
 dolly_mock_set_nsd(1);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 r=dolly_block_read(s,UINT64_C(0x100000200),buf,512);
 assert(r.status==DOLLY_BLOCK_OK && dolly_mock_last_read64());
 assert(!dolly_mock_last_scsi_lba());
 dolly_block_close(s);
 opt.capacity_bytes=opt.required_read_end=UINT64_C(0x2000000200);
 dolly_mock_set_nsd(0);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EUNSUPPORTED && s==NULL);
 /* ROM profile validates the opened device, uses Chip RAM, and
  * requires both a modern version and an advertised READ64 command. */
 opt.device_name="scsi.device";
 opt.verified_uae_ide=0;
 opt.require_rom_scsi=1;
 opt.capacity_bytes=UINT64_C(0x200000000);
 opt.required_read_end=UINT64_C(0x100000000);
 dolly_mock_set_rom_scsi_version(0);
 assert(!dolly_amiga_probe_rom_scsi(0));
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EDEVICE && s==NULL);
 dolly_mock_set_rom_scsi_version(40);
 dolly_mock_set_nsd(1);
 assert(dolly_amiga_probe_rom_scsi(0));
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 assert(dolly_mock_last_alloc_flags() & (1u<<1));
 r=dolly_block_read(s,UINT64_C(0xfffffe00),buf,512);
 assert(r.status==DOLLY_BLOCK_OK && !dolly_mock_last_read64());
 r=dolly_block_read(s,UINT64_C(0xfffffe00),buf,1024);
 assert(r.status==DOLLY_BLOCK_EUNSUPPORTED && r.bytes_read==0);
 dolly_block_close(s);
 opt.required_read_end+=512;
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EUNSUPPORTED && s==NULL);
 opt.bypass_legacy_scsi_limit=1;
 dolly_mock_set_nsd(0);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EUNSUPPORTED && s==NULL);
 dolly_mock_set_nsd(1);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 r=dolly_block_read(s,UINT64_C(0x100000000),buf,512);
 assert(r.status==DOLLY_BLOCK_OK && dolly_mock_last_read64());
 dolly_block_close(s);
 opt.bypass_legacy_scsi_limit=0;
 dolly_mock_set_rom_scsi_version(43);
 dolly_mock_set_nsd(2);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_EUNSUPPORTED && s==NULL);
 dolly_mock_set_nsd(1);
 assert(dolly_amiga_open_device(&opt,&s)==DOLLY_BLOCK_OK);
 r=dolly_block_read(s,UINT64_C(0x100000000),buf,512);
 assert(r.status==DOLLY_BLOCK_OK && dolly_mock_last_read64());
 assert(!dolly_mock_last_scsi_lba());
 dolly_block_close(s);
 assert(dma_report_count==3); /* No DMA warnings during valid-profile reads. */
 puts("Mock Exec backend checks passed");return 0;
}
