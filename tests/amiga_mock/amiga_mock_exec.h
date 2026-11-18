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
#define HD_SCSICMD 28u
#define SCSIF_READ 1u
#define SCSIF_AUTOSENSE 2u
struct SCSICmd {
 UWORD *scsi_Data;
 ULONG scsi_Length,scsi_Actual;
 UBYTE *scsi_Command;
 UWORD scsi_CmdLength;
 UBYTE scsi_Flags,scsi_Status;
 UBYTE *scsi_SenseData;
 UWORD scsi_SenseLength,scsi_SenseActual;
};
struct MsgPort *CreateMsgPort(void);
void DeleteMsgPort(struct MsgPort *port);
struct IORequest *CreateIORequest(struct MsgPort *port,ULONG size);
void DeleteIORequest(struct IORequest *req);
LONG OpenDevice(CONST_STRPTR name,ULONG unit,struct IORequest *req,ULONG flags);
void CloseDevice(struct IORequest *req);
LONG DoIO(struct IORequest *req);
APTR AllocMem(ULONG bytes,ULONG flags);
void FreeMem(APTR p,ULONG bytes);
unsigned dolly_mock_rom_scsi_version(void);
void dolly_mock_set_rom_scsi_version(unsigned version);
void dolly_mock_set_nsd(int enabled);
int dolly_mock_last_read64(void);
uint32_t dolly_mock_last_scsi_lba(void);
uint64_t dolly_mock_dma_address(void);
void dolly_mock_set_dma_address(uint64_t address);
ULONG dolly_mock_last_alloc_flags(void);
/* Fault injection: fail read operation number N (1-based).
 * mode: 0 disabled, 1 short transfer, 2 I/O error,
 * 3 SCSI illegal request, 4 SCSI medium error. */
void dolly_mock_read_fault(int operation_number,int mode);
#endif
