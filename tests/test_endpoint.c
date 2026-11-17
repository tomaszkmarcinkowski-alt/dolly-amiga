#include "dolly_endpoint.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    DollyEndpoint src={0},dst={0};
    assert(!dolly_demo_can_start(DOLLY_DEMO_SCAN,&src,&dst));
    assert(dolly_endpoint_set_unit(&src,0,0));
    assert(dolly_demo_can_start(DOLLY_DEMO_SCAN,&src,&dst));
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_unit(&dst,1,0));
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(!dolly_demo_can_start(DOLLY_DEMO_RESTORE,&src,&dst));
    assert(dolly_endpoint_set_unit(&dst,0,0));
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(!dolly_endpoint_set_partition(&src,0,0,9));
    assert(src.kind==DOLLY_ENDPOINT_UNIT);
    assert(dolly_endpoint_set_partition(&src,0,0,0));
    assert(dolly_endpoint_set_partition(&dst,0,0,1));
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_file(&src,"RAM:test.img"));
    assert(dolly_endpoint_set_file(&dst,"RAM:test.img"));
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_file(&dst,"RAM:other.img"));
    assert(dolly_demo_can_start(DOLLY_DEMO_RESTORE,&src,&dst));
    assert(!dolly_endpoint_set_file(&dst,""));
    assert(!dolly_endpoint_set_unit(&dst,2,0));
    assert(dolly_demo_needs_target(DOLLY_DEMO_COPY));
    assert(!dolly_demo_needs_target(DOLLY_DEMO_SCAN));
    puts("endpoint selection: PASS");
    return 0;
}
