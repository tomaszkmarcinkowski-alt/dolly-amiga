#include "dolly_operation_session.h"
#include <string.h>

static uint64_t bytes_per_second(uint64_t bytes,uint64_t ticks,uint32_t frequency) {
    if(!ticks || !frequency)return 0;
    while(bytes>UINT64_MAX/frequency) {
        if(ticks<2u)return UINT64_MAX;
        bytes/=2u;
        ticks/=2u;
    }
    return bytes*frequency/ticks;
}

void dolly_operation_timing_start(DollyOperationTiming *t,uint64_t now,uint32_t frequency) {
    memset(t,0,sizeof(*t));
    t->frequency=frequency;
    t->start=now;
}

uint64_t dolly_operation_timing_elapsed(const DollyOperationTiming *t,uint64_t now) {
    return (t->finished?t->end:now)-t->start;
}

uint64_t dolly_operation_timing_work(const DollyOperationTiming *t,uint64_t now) {
    return t->active_ticks+(t->active?now-t->active_start:0);
}

void dolly_operation_timing_active(DollyOperationTiming *t,uint64_t now,uint64_t bytes,int active) {
    if(!t->frequency || t->finished || t->active==active)return;
    t->active_ticks=dolly_operation_timing_work(t,now);
    t->active_start=now;
    t->active=active;
    t->sample_ticks=t->active_ticks;
    t->sample_bytes=bytes;
    t->rate_valid=0;
}

void dolly_operation_timing_sample(DollyOperationTiming *t,uint64_t now,uint64_t bytes) {
    if(!t->active)return;
    uint64_t work=dolly_operation_timing_work(t,now),interval=work-t->sample_ticks;
    if(bytes<t->sample_bytes)t->rate_valid=0;
    else {
        if(interval<t->frequency)return;
        t->rate=bytes_per_second(bytes-t->sample_bytes,interval,t->frequency);
        t->rate_valid=1;
    }
    t->sample_ticks=work;
    t->sample_bytes=bytes;
}

void dolly_operation_timing_finish(DollyOperationTiming *t,uint64_t now,uint64_t bytes) {
    if(t->finished)return;
    dolly_operation_timing_active(t,now,bytes,0);
    t->end=now;
    t->finished=1;
}

uint64_t dolly_operation_timing_average(const DollyOperationTiming *t,uint64_t now,uint64_t bytes) {
    return bytes_per_second(bytes,dolly_operation_timing_work(t,now),t->frequency);
}

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

DollyOperationResult dolly_operation_rewind(DollyOperationSession *s,uint64_t completed) {
    if (!s) return DOLLY_OP_INVALID;
    if (s->state != DOLLY_OP_RUNNING) return DOLLY_OP_STATE_ERROR;
    if (completed > s->completed_blocks) return DOLLY_OP_RANGE_ERROR;
    s->completed_blocks = completed;
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
