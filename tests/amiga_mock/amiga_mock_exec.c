#include "amiga_mock_exec.h"
#include <stdlib.h>
#include <string.h>
static int g_nsd;
static int g_last_read64;
static uint64_t g_dma_address=UINT64_C(0x00100000);
static ULONG g_last_alloc_flags;
static UWORD g_commands[]={2,0xc000,0};
static UWORD g_legacy_commands[]={2,0};
static int g_read_count,g_fault_number,g_fault_mode;
struct NSQueryMock {ULONG format,size;UWORD type,subtype;UWORD *cmds;};
void dolly_mock_set_nsd(int enabled){g_nsd=enabled;g_last_read64=0;g_read_count=0;}
int dolly_mock_last_read64(void){return g_last_read64;}
uint64_t dolly_mock_dma_address(void){return g_dma_address;}
void dolly_mock_set_dma_address(uint64_t address){g_dma_address=address;}
ULONG dolly_mock_last_alloc_flags(void){return g_last_alloc_flags;}
void dolly_mock_read_fault(int number,int mode){g_fault_number=number;g_fault_mode=mode;g_read_count=0;}
struct MsgPort *CreateMsgPort(void){return calloc(1,sizeof(struct MsgPort));}
void DeleteMsgPort(struct MsgPort *p){free(p);}
struct IORequest *CreateIORequest(struct MsgPort *p,ULONG sz){(void)p;(void)sz;return calloc(1,sizeof(struct IOStdReq)+128);}
void DeleteIORequest(struct IORequest *p){free(p);}
LONG OpenDevice(CONST_STRPTR n,ULONG unit,struct IORequest *r,ULONG flags){(void)r;(void)flags;(void)unit;return strcmp(n,"mock.device")==0?0:-1;}
void CloseDevice(struct IORequest *r){(void)r;}
APTR AllocMem(ULONG bytes,ULONG flags){g_last_alloc_flags=flags;return malloc(bytes);}
void FreeMem(APTR p,ULONG bytes){(void)bytes;free(p);}
LONG DoIO(struct IORequest *r){
 ULONG i;uint64_t start;
 if(r->io_Command==0x4000){
  struct NSQueryMock q; if(!g_nsd){r->io_Error=255;return -1;}
  q.format=0;q.size=sizeof(q);q.type=5;q.subtype=0;
  q.cmds=g_nsd==2?g_legacy_commands:g_commands;
  memcpy(r->io_Data,&q,sizeof(q));r->io_Actual=sizeof(q);r->io_Error=0;return 0;
 }
 if(r->io_Command!=2 && r->io_Command!=0xc000){r->io_Error=255;return -1;}
 ++g_read_count;
 if(g_fault_number==g_read_count && g_fault_mode==2){
  r->io_Error=1;r->io_Actual=0;return -1;
 }
 if(g_fault_number==g_read_count && g_fault_mode==1){
  r->io_Error=0;r->io_Actual=r->io_Length/2;return 0;
 }
 g_last_read64=(r->io_Command==0xc000);
 start=(uint64_t)r->io_Offset | (g_last_read64?((uint64_t)r->io_Actual<<32):0);
 for(i=0;i<r->io_Length;i++)((UBYTE *)r->io_Data)[i]=(UBYTE)((start+i)&255u);
 r->io_Actual=r->io_Length;r->io_Error=0;return 0;
}
