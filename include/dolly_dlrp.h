#ifndef DOLLY_DLRP_H
#define DOLLY_DLRP_H
#include "dolly_dly_writer.h"

/* Streaming IFF output. Integers are explicitly big-endian. */
void dolly_dlrp_u32(unsigned char *out,uint32_t value);
void dolly_dlrp_u64(unsigned char *out,uint64_t value);
int dolly_dlrp_form(DollyDlySink sink,void *ctx,uint32_t body_bytes);
/* Include the chunk's header and IFF padding when accounting for body size. */
int dolly_dlrp_chunk(DollyDlySink sink,void *ctx,const char id[4],
                     const void *data,uint32_t bytes);
#endif
