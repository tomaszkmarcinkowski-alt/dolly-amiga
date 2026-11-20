#include "dolly_operation_session.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

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
    puts("OperationSession: PASS");
    return 0;
}
