#include "dolly_amiga_clipboard.h"
#include <devices/clipboard.h>
#include <proto/exec.h>
#include <stdlib.h>
#include <string.h>

static void write_be32(UBYTE *dest,ULONG value) {
    dest[0]=(UBYTE)(value>>24);
    dest[1]=(UBYTE)(value>>16);
    dest[2]=(UBYTE)(value>>8);
    dest[3]=(UBYTE)value;
}
int dolly_amiga_copy_text(const char *row) {
    struct MsgPort *port;
    struct IOClipReq *io;
    UBYTE *data;
    ULONG length,padded;
    int copied=0;
    if(!row)return 0;
    length=(ULONG)strlen(row);
    padded=length+(length&1u);
    data=malloc(20u+padded);
    if(!data)return 0;
    memcpy(data,"FORM",4);
    write_be32(data+4,12u+padded);
    memcpy(data+8,"FTXTCHRS",8);
    write_be32(data+16,length);
    memcpy(data+20,row,length);
    if(padded!=length)data[20+length]=0;
    port=CreateMsgPort();
    if(port) {
        io=(struct IOClipReq *)CreateIORequest(port,sizeof(*io));
        if(io) {
            if(OpenDevice((STRPTR)"clipboard.device",0,(struct IORequest *)io,0)==0) {
                io->io_Offset=0;
                io->io_ClipID=0;
                io->io_Data=(STRPTR)data;
                io->io_Length=20u+padded;
                io->io_Command=CMD_WRITE;
                if(DoIO((struct IORequest *)io)==0 && io->io_Actual==20u+padded) {
                    io->io_Command=CMD_UPDATE;
                    copied=DoIO((struct IORequest *)io)==0;
                }
                CloseDevice((struct IORequest *)io);
            }
            DeleteIORequest((struct IORequest *)io);
        }
        DeleteMsgPort(port);
    }
    free(data);
    return copied;
}
