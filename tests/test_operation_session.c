#include "dolly_operation_session.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_timing(void) {
    DollyOperationTiming t;
    dolly_operation_timing_start(&t,100,1000);
    assert(dolly_operation_timing_elapsed(&t,2100)==2000); /* Preflight or validation. */
    assert(dolly_operation_timing_work(&t,2100)==0);
    dolly_operation_timing_active(&t,2100,0,1);
    dolly_operation_timing_sample(&t,2600,512);
    assert(!t.rate_valid);
    dolly_operation_timing_sample(&t,3100,1024);
    assert(t.rate_valid && t.rate==1024);
    dolly_operation_timing_active(&t,3600,1536,0);
    assert(!t.rate_valid && dolly_operation_timing_work(&t,7600)==1500);
    dolly_operation_timing_active(&t,7600,512,1); /* Retry from an earlier offset. */
    dolly_operation_timing_sample(&t,8600,1536);
    assert(t.rate_valid && t.rate==1024);
    dolly_operation_timing_sample(&t,8700,1024);
    assert(!t.rate_valid);
    dolly_operation_timing_finish(&t,9100,2048);
    assert(dolly_operation_timing_elapsed(&t,20000)==9000);
    assert(dolly_operation_timing_work(&t,20000)==3000);
    assert(dolly_operation_timing_average(&t,20000,2048)==682);
    dolly_operation_timing_start(&t,0,1000);
    dolly_operation_timing_active(&t,0,0,1);
    dolly_operation_timing_finish(&t,250,1024);
    assert(dolly_operation_timing_average(&t,1000,1024)==4096);
    dolly_operation_timing_start(&t,0,0);
    dolly_operation_timing_active(&t,0,0,1);
    assert(!t.active && dolly_operation_timing_average(&t,1000,1024)==0);
}

int main(void) {
    DollyOperationSession s;
    dolly_operation_init(&s);
    assert(s.state == DOLLY_OP_IDLE);
    assert(dolly_operation_start(&s, 192) == DOLLY_OP_OK);
    assert(dolly_operation_progress(&s, 48, 1, 2) == DOLLY_OP_OK);
    dolly_operation_show_sheep_counter(&s);
    dolly_operation_hide_sheep_counter(&s);
    /* Closing Sheep Counter never changes the operation. */
    assert(s.state == DOLLY_OP_RUNNING);
    assert(s.completed_blocks == 48 && s.errors == 1 && s.retries == 2);
    assert(s.sheep_counter_visible == 0);
    assert(dolly_operation_progress(&s, 96, 1, 2) == DOLLY_OP_OK);
    dolly_operation_show_sheep_counter(&s);
    assert(s.sheep_counter_visible == 1 && s.completed_blocks == 96);
    assert(dolly_operation_progress(&s, 95, 1, 2) == DOLLY_OP_RANGE_ERROR);
    assert(dolly_operation_progress(&s, 193, 1, 2) == DOLLY_OP_RANGE_ERROR);
    assert(dolly_operation_rewind(&s, 97) == DOLLY_OP_RANGE_ERROR);
    assert(dolly_operation_rewind(&s, 64) == DOLLY_OP_OK);
    assert(s.completed_blocks == 64 && s.errors == 1 && s.retries == 2);
    assert(dolly_operation_progress(&s, 80, 1, 2) == DOLLY_OP_OK);
    assert(dolly_operation_pause(&s) == DOLLY_OP_OK);
    assert(dolly_operation_progress(&s, 100, 1, 2) == DOLLY_OP_STATE_ERROR);
    assert(dolly_operation_rewind(&s, 64) == DOLLY_OP_STATE_ERROR);
    assert(dolly_operation_resume(&s) == DOLLY_OP_OK);
    assert(dolly_operation_finish(&s, 1) == DOLLY_OP_RANGE_ERROR);
    assert(dolly_operation_progress(&s, 192, 1, 2) == DOLLY_OP_OK);
    assert(dolly_operation_finish(&s, 1) == DOLLY_OP_OK);
    assert(dolly_operation_progress(&s, 192, 1, 2) == DOLLY_OP_STATE_ERROR);
    assert(dolly_operation_start(&s, UINT64_C(0x100000001)) == DOLLY_OP_OK);
    assert(s.completed_blocks == 0 && s.errors == 0 && s.retries == 0);
    assert(dolly_operation_cancel(&s) == DOLLY_OP_OK);
    assert(s.state == DOLLY_OP_CANCELLED);
    assert(dolly_operation_start(&s, 8) == DOLLY_OP_OK);
    assert(dolly_operation_finish(&s, 0) == DOLLY_OP_OK);
    test_timing();
    puts("OperationSession: PASS");
    return 0;
}
