#ifndef DOLLY_BLOCK_SOURCE_H
#define DOLLY_BLOCK_SOURCE_H
/* M0: host read-only byte-addressed I/O. Native Amiga adapter pending. */
#include <stddef.h>
#include <stdint.h>
typedef struct DollyBlockSource DollyBlockSource;
typedef enum DollyBlockStatus {
 DOLLY_BLOCK_OK=0, DOLLY_BLOCK_EINVAL, DOLLY_BLOCK_ERANGE,
 DOLLY_BLOCK_ESHORT, DOLLY_BLOCK_EIO, DOLLY_BLOCK_ENOMEM,
 DOLLY_BLOCK_EUNSUPPORTED
} DollyBlockStatus;
typedef struct DollyBlockReadResult {
 DollyBlockStatus status;
 size_t bytes_read;
} DollyBlockReadResult;
DollyBlockStatus dolly_block_open_file(const char *path,DollyBlockSource **out);
DollyBlockReadResult dolly_block_read(DollyBlockSource *src,uint64_t offset,void *buffer,size_t bytes);
uint64_t dolly_block_size(const DollyBlockSource *src);
void dolly_block_close(DollyBlockSource *src);
const char *dolly_block_status_name(DollyBlockStatus status);
#endif
