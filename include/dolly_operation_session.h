#ifndef DOLLY_OPERATION_SESSION_H
#define DOLLY_OPERATION_SESSION_H

/* Pure C99 operation-state model. No AmigaOS, POSIX or device I/O.
 * A view may disappear while its underlying operation continues.
 */
#include <stdint.h>

typedef enum DollyOperationState {
    DOLLY_OP_IDLE = 0,
    DOLLY_OP_RUNNING = 1,
    DOLLY_OP_PAUSED = 2,
    DOLLY_OP_COMPLETED = 3,
    DOLLY_OP_FAILED = 4,
    DOLLY_OP_CANCELLED = 5
} DollyOperationState;

typedef enum DollyOperationResult {
    DOLLY_OP_OK = 0,
    DOLLY_OP_INVALID = 1,
    DOLLY_OP_STATE_ERROR = 2,
    DOLLY_OP_RANGE_ERROR = 3
} DollyOperationResult;

typedef struct DollyOperationSession {
    uint64_t total_blocks;
    uint64_t completed_blocks;
    uint32_t errors;
    uint32_t retries;
    DollyOperationState state;
    unsigned char sheep_counter_visible;
} DollyOperationSession;

/* Tick values come from a monotonic clock. Payload bytes exclude retries,
 * skipped sectors, validation passes and format overhead. */
typedef struct DollyOperationTiming {
    uint64_t start,end,active_start,active_ticks,sample_ticks,sample_bytes,rate;
    uint32_t frequency;
    int active,finished,rate_valid;
} DollyOperationTiming;

void dolly_operation_timing_start(DollyOperationTiming *timing,uint64_t now,uint32_t frequency);
void dolly_operation_timing_active(DollyOperationTiming *timing,uint64_t now,
                                   uint64_t bytes,int active);
void dolly_operation_timing_sample(DollyOperationTiming *timing,uint64_t now,uint64_t bytes);
void dolly_operation_timing_finish(DollyOperationTiming *timing,uint64_t now,uint64_t bytes);
uint64_t dolly_operation_timing_elapsed(const DollyOperationTiming *timing,uint64_t now);
uint64_t dolly_operation_timing_work(const DollyOperationTiming *timing,uint64_t now);
uint64_t dolly_operation_timing_average(const DollyOperationTiming *timing,uint64_t now,
                                        uint64_t bytes);

void dolly_operation_init(DollyOperationSession *session);
DollyOperationResult dolly_operation_start(DollyOperationSession *session,
                                          uint64_t total_blocks);
DollyOperationResult dolly_operation_progress(DollyOperationSession *session,
                                             uint64_t completed_blocks,
                                             uint32_t errors,
                                             uint32_t retries);
/* Explicitly discard in-progress blocks when retrying an output part. */
DollyOperationResult dolly_operation_rewind(DollyOperationSession *session,uint64_t completed);
DollyOperationResult dolly_operation_pause(DollyOperationSession *session);
DollyOperationResult dolly_operation_resume(DollyOperationSession *session);
DollyOperationResult dolly_operation_finish(DollyOperationSession *session,
                                           int success);
/* Only the core operation manager may request cancellation. */
DollyOperationResult dolly_operation_cancel(DollyOperationSession *session);
/* These are view-only actions and never change operation state. */
void dolly_operation_show_sheep_counter(DollyOperationSession *session);
void dolly_operation_hide_sheep_counter(DollyOperationSession *session);

#endif
