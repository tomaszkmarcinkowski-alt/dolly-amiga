#include "dolly_operation_session.h"
#include <string.h>

void dolly_operation_init(DollyOperationSession *s) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->state = DOLLY_OP_IDLE;
}

DollyOperationResult dolly_operation_start(DollyOperationSession *s,
                                           uint64_t total) {
    if (!s || total == 0) return DOLLY_OP_INVALID;
    if (s->state == DOLLY_OP_RUNNING || s->state == DOLLY_OP_PAUSED)
        return DOLLY_OP_STATE_ERROR;
    /* Reset all per-run metrics. Visibility is a user preference. */
    s->total_blocks = total;
    s->completed_blocks = 0;
    s->errors = 0;
    s->retries = 0;
    s->state = DOLLY_OP_RUNNING;
    return DOLLY_OP_OK;
}

DollyOperationResult dolly_operation_progress(DollyOperationSession *s,
                                              uint64_t completed,
                                              uint32_t errors,
                                              uint32_t retries) {
    if (!s) return DOLLY_OP_INVALID;
    if (s->state != DOLLY_OP_RUNNING) return DOLLY_OP_STATE_ERROR;
    if (completed < s->completed_blocks || completed > s->total_blocks ||
        errors < s->errors || retries < s->retries)
        return DOLLY_OP_RANGE_ERROR;
    s->completed_blocks = completed;
    s->errors = errors;
    s->retries = retries;
    return DOLLY_OP_OK;
}

DollyOperationResult dolly_operation_pause(DollyOperationSession *s) {
    if (!s) return DOLLY_OP_INVALID;
    if (s->state != DOLLY_OP_RUNNING) return DOLLY_OP_STATE_ERROR;
    s->state = DOLLY_OP_PAUSED;
    return DOLLY_OP_OK;
}

DollyOperationResult dolly_operation_resume(DollyOperationSession *s) {
    if (!s) return DOLLY_OP_INVALID;
    if (s->state != DOLLY_OP_PAUSED) return DOLLY_OP_STATE_ERROR;
    s->state = DOLLY_OP_RUNNING;
    return DOLLY_OP_OK;
}

DollyOperationResult dolly_operation_finish(DollyOperationSession *s,
                                            int success) {
    if (!s) return DOLLY_OP_INVALID;
    if (s->state != DOLLY_OP_RUNNING && s->state != DOLLY_OP_PAUSED)
        return DOLLY_OP_STATE_ERROR;
    if (success && s->completed_blocks != s->total_blocks)
        return DOLLY_OP_RANGE_ERROR;
    s->state = success ? DOLLY_OP_COMPLETED : DOLLY_OP_FAILED;
    return DOLLY_OP_OK;
}

DollyOperationResult dolly_operation_cancel(DollyOperationSession *s) {
    if (!s) return DOLLY_OP_INVALID;
    if (s->state != DOLLY_OP_RUNNING && s->state != DOLLY_OP_PAUSED)
        return DOLLY_OP_STATE_ERROR;
    s->state = DOLLY_OP_CANCELLED;
    return DOLLY_OP_OK;
}

void dolly_operation_show_sheep_counter(DollyOperationSession *s) {
    if (s) s->sheep_counter_visible = 1;
}

void dolly_operation_hide_sheep_counter(DollyOperationSession *s) {
    if (s) s->sheep_counter_visible = 0;
}
