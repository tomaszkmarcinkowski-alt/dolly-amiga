#ifndef DOLLY_DLRP_H
#define DOLLY_DLRP_H
#include "dolly_dly_writer.h"
#include "dolly_endpoint.h"

typedef int (*DollyDlrpRead)(void *context,void *data,size_t bytes);

typedef struct DollyDlrpImageResume {
    DollyEndpoint source,target;
    char driver[60],device_name[112],partial[256];
    uint64_t first_lba,sectors,completed,capacity;
    uint32_t unit,sector_bytes,output_bytes;
    uint32_t batch_bytes,dly_file_bytes;
    DollyDlyCodec codec;
    uint32_t capacity_source,cylinders,heads,sectors_per_track;
    uint32_t anchors,first_crc,last_crc;
} DollyDlrpImageResume;

typedef struct DollyDlrpTransferExtent {
    DollyEndpoint endpoint;
    char driver[60],name[112];
    uint64_t start,length,capacity;
    uint32_t unit,first_crc,last_crc;
    uint32_t capacity_source,cylinders,heads,sectors_per_track;
} DollyDlrpTransferExtent;

typedef struct DollyDlrpTransferResume {
    DollyDlrpTransferExtent source,target;
    uint64_t committed,uncertain,bytes;
    uint32_t sector_bytes,batch_bytes;
    int restoring,verify;
} DollyDlrpTransferResume;

typedef enum DollyDlrpStatus {
    DOLLY_DLRP_INVALID,
    DOLLY_DLRP_UNSUPPORTED,
    DOLLY_DLRP_READY
} DollyDlrpStatus;

/* Decode a complete version-1 single-file checkpoint. The reader must return
 * a nonzero result only after delivering the requested bytes. */
DollyDlrpStatus dolly_dlrp_read_image(DollyDlrpRead read,void *context,uint32_t length,
                                     DollyDlrpImageResume *resume);
DollyDlrpStatus dolly_dlrp_read_transfer(DollyDlrpRead read,void *context,uint32_t length,
                                        DollyDlrpTransferResume *resume);

/* Streaming IFF output. Integers are explicitly big-endian. */
void dolly_dlrp_u32(unsigned char *out,uint32_t value);
void dolly_dlrp_u64(unsigned char *out,uint64_t value);
int dolly_dlrp_form(DollyDlySink sink,void *ctx,uint32_t body_bytes);
/* Include the chunk's header and IFF padding when accounting for body size. */
int dolly_dlrp_chunk(DollyDlySink sink,void *ctx,const char id[4],
                     const void *data,uint32_t bytes);
#endif
