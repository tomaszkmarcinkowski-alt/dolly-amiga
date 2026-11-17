#ifndef DOLLY_AMIGA_MOCK_EXEC_H
#define DOLLY_AMIGA_MOCK_EXEC_H
#include <stddef.h>
#include <stdint.h>
typedef uint32_t ULONG;typedef uint16_t UWORD;typedef uint8_t UBYTE;
typedef int32_t LONG;typedef void *APTR;typedef const char *CONST_STRPTR;
struct MsgPort {int token;};
struct IOStdReq {
 UWORD io_Command;UBYTE io_Flags;UBYTE io_Error;
 ULONG io_Actual,io_Offset,io_Length;APTR io_Data;
};
struct IORequest {UWORD io_Command;UBYTE io_Flags;UBYTE io_Error;
 ULONG io_Actual,io_Offset,io_Length;APTR io_Data;};
struct MsgPort *CreateMsgPort(void);
void DeleteMsgPort(struct MsgPort *port);
struct IORequest *CreateIORequest(struct MsgPort *port,ULONG size);
void DeleteIORequest(struct IORequest *req);
LONG OpenDevice(CONST_STRPTR name,ULONG unit,struct IORequest *req,ULONG flags);
void CloseDevice(struct IORequest *req);
LONG DoIO(struct IORequest *req);
APTR AllocMem(ULONG bytes,ULONG flags);
void FreeMem(APTR p,ULONG bytes);
void dolly_mock_set_nsd(int enabled);
int dolly_mock_last_read64(void);
#endif
