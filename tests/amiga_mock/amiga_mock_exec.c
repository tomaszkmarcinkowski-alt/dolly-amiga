#include "amiga_mock_exec.h"
#include <stdlib.h>
#include <string.h>
static int g_nsd;
static int g_last_read64;
static UWORD g_commands[]={2,0xc000,0};
struct NSQueryMock {ULONG format,size;UWORD type,subtype;UWORD *cmds;};
void dolly_mock_set_nsd(int enabled){g_nsd=enabled;g_last_read64=0;}
int dolly_mock_last_read64(void){return g_last_read64;}
struct MsgPort *CreateMsgPort(void){return calloc(1,sizeof(struct MsgPort));}
void DeleteMsgPort(struct MsgPort *p){free(p);}
struct IORequest *CreateIORequest(struct MsgPort *p,ULONG sz){(void)p;(void)sz;return calloc(1,sizeof(struct IOStdReq)+128);}
void DeleteIORequest(struct IORequest *p){free(p);}
LONG OpenDevice(CONST_STRPTR n,ULONG unit,struct IORequest *r,ULONG flags){(void)r;(void)flags;(void)unit;return strcmp(n,"mock.device")==0?0:-1;}
void CloseDevice(struct IORequest *r){(void)r;}
APTR AllocMem(ULONG bytes,ULONG flags){(void)flags;return malloc(bytes);}
void FreeMem(APTR p,ULONG bytes){(void)bytes;free(p);}
LONG DoIO(struct IORequest *r){
 ULONG i;uint64_t start;
 if(r->io_Command==0x4000){
  struct NSQueryMock q; if(!g_nsd){r->io_Error=255;return -1;}
  q.format=0;q.size=sizeof(q);q.type=5;q.subtype=0;q.cmds=g_commands;
  memcpy(r->io_Data,&q,sizeof(q));r->io_Actual=sizeof(q);r->io_Error=0;return 0;
 }
 if(r->io_Command!=2 && r->io_Command!=0xc000){r->io_Error=255;return -1;}
 g_last_read64=(r->io_Command==0xc000);
 start=(uint64_t)r->io_Offset | (g_last_read64?((uint64_t)r->io_Actual<<32):0);
 for(i=0;i<r->io_Length;i++)((UBYTE *)r->io_Data)[i]=(UBYTE)((start+i)&255u);
 r->io_Actual=r->io_Length;r->io_Error=0;return 0;
}
