#ifndef DOLLY_RESTORE_TRANSFER_H
#define DOLLY_RESTORE_TRANSFER_H
#include "dolly_block_target.h"

typedef enum DollyRestoreTransferState {
    DOLLY_RESTORE_TRANSFERRING,
    DOLLY_RESTORE_FINISHED,
    DOLLY_RESTORE_READ_FAILED,
    DOLLY_RESTORE_WRITE_FAILED
} DollyRestoreTransferState;

typedef DollyBlockWriteStatus (*DollyRestoreWriteBatch)(void *target,uint64_t offset,
                                       const void *data,size_t bytes,int verify);

/* Initialized only after preflight, confirmation and destination revalidation
 * by the caller. Buffers, source and target remain owned by that caller. */
typedef struct DollyRestoreTransfer {
    DollyBlockSource *source;
    void *target;
    DollyRestoreWriteBatch write_batch;
    uint64_t bytes,processed;
    /* Nonzero check limit gates writes until the entire source is read. */
    uint64_t source_check_bytes,source_checked;
    uint32_t batch_bytes;
    int verify;
    DollyRestoreTransferState state;
    DollyBlockStatus read_status;
    DollyBlockWriteStatus write_status;
} DollyRestoreTransfer;

int dolly_restore_transfer_init(DollyRestoreTransfer *transfer,DollyBlockSource *source,
                      uint64_t target_bytes,uint32_t sector_bytes,uint32_t max_transfer,
                      void *target,DollyRestoreWriteBatch write_batch,int verify);
/* After explicit size approval, transfer only the common prefix. */
int dolly_restore_transfer_init_prefix(DollyRestoreTransfer *transfer,DollyBlockSource *source,
                      uint64_t target_bytes,uint32_t sector_bytes,uint32_t max_transfer,
                      void *target,DollyRestoreWriteBatch write_batch,int verify);
/* Up to one batch. A failed batch is terminal and processed remains the
 * offset of that batch, even if the device may have written some of it. */
DollyRestoreTransferState dolly_restore_transfer_step(DollyRestoreTransfer *transfer,
                                                     void *buffer,size_t buffer_bytes);
#endif
