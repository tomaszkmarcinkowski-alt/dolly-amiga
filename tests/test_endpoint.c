#include "dolly_endpoint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
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
    assert(dolly_endpoint_set_partition(&src,0,0,9));
    assert(src.kind==DOLLY_ENDPOINT_PARTITION);
    assert(dolly_endpoint_set_partition(&src,0,0,0));
    assert(dolly_endpoint_set_partition(&dst,0,0,1));
    assert(dolly_endpoint_set_partition(&dst,0,0,15));
    assert(!dolly_endpoint_set_partition(&dst,0,0,16));
    assert(dolly_endpoint_set_partition(&dst,0,0,1));
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_file(&src,"RAM:test.img"));
    assert(dolly_endpoint_set_file(&dst,"RAM:test.img"));
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_file(&dst,"RAM:other.img"));
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(!dolly_demo_can_start(DOLLY_DEMO_RESTORE,&src,&dst));
    assert(dolly_endpoint_mode(&src)==DOLLY_MODE_FILE);
    assert(dolly_demo_needs_target(DOLLY_DEMO_CREATE_IMAGE));
    assert(!dolly_demo_can_start(DOLLY_DEMO_CREATE_IMAGE,&src,&dst));
    assert(dolly_endpoint_set_unit(&dst,1,0));
    assert(dolly_demo_can_start(DOLLY_DEMO_RESTORE,&src,&dst));
    assert(!dolly_demo_can_start(DOLLY_DEMO_CREATE_IMAGE,&src,&dst));
    assert(dolly_endpoint_set_file(&dst,"RAM:created.img"));
    assert(dst.image_format==DOLLY_IMAGE_RAW);
    dst.image_format=DOLLY_IMAGE_IFF_DOLY;
    assert(dst.kind==DOLLY_ENDPOINT_FILE);
    assert(dst.image_format==DOLLY_IMAGE_IFF_DOLY);
    assert(!dolly_demo_can_start(DOLLY_DEMO_CREATE_IMAGE,&src,&dst));
    assert(dolly_endpoint_set_logical(&src,0));
    assert(dolly_endpoint_mode(&src)==DOLLY_MODE_LOGICAL);
    assert(dolly_demo_can_start(DOLLY_DEMO_CREATE_IMAGE,&src,&dst));
    assert(dolly_endpoint_set_logical(&dst,0));
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_logical(&dst,1));
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_logical(&src,23));
    assert(!dolly_endpoint_set_logical(&src,24));
    assert(dolly_endpoint_set_logical(&src,0));
    assert(dolly_endpoint_set_logical(&dst,1));
    strcpy(src.path,"DH0:");
    strcpy(dst.path,"DH0:");
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    strcpy(dst.path,"DH1:");
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(!dolly_endpoint_set_file(&dst,""));
    assert(dolly_endpoint_set_unit(&dst,2,0));
    assert(!dolly_endpoint_set_unit(&dst,16,0));
    assert(dolly_endpoint_set_unit(&src,0,0));
    assert(dolly_endpoint_set_unit(&dst,1,0));
    strcpy(src.path,"scsi.device / Unit 0");
    strcpy(dst.path,"scsi.device / Unit 0");
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    strcpy(dst.path,"uaehf.device / Unit 0");
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_partition(&dst,1,0,2));
    strcpy(dst.path,"scsi.device / Unit 0 / DH0");
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    strcpy(dst.path,"scsi.device / Unit 1 / DH0");
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    strcpy(dst.path,"scsi.device / Unit 10 / DH0");
    assert(!dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_endpoint_set_unit(&dst,1,0));
    strcpy(dst.path,"scsi.device / Unit 10");
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(dolly_demo_needs_target(DOLLY_DEMO_COPY));
    assert(!dolly_demo_needs_target(DOLLY_DEMO_SCAN));
    /* Operation drives the available Source/Destination picker pages.
     * Cycle indices change, but stable DollyEndpointMode IDs do not.
     */
    {
        unsigned m;
        m=dolly_allowed_modes(DOLLY_DEMO_RESTORE,0,
                              DOLLY_ENDPOINT_NONE,0);
        assert(m==DOLLY_MODE_BIT(DOLLY_MODE_FILE));
        m=dolly_allowed_modes(DOLLY_DEMO_RESTORE,1,
                              DOLLY_ENDPOINT_FILE,0);
        assert(dolly_mode_allowed(m,DOLLY_MODE_PHYSICAL));
        assert(dolly_mode_allowed(m,DOLLY_MODE_PARTITION));
        assert(!dolly_mode_allowed(m,DOLLY_MODE_FILE));
        m=dolly_allowed_modes(DOLLY_DEMO_COPY,1,
                              DOLLY_ENDPOINT_UNIT,0);
        assert(m==DOLLY_MODE_BIT(DOLLY_MODE_PHYSICAL));
        m=dolly_allowed_modes(DOLLY_DEMO_COPY,1,
                              DOLLY_ENDPOINT_UNIT,1);
        assert(dolly_mode_allowed(m,DOLLY_MODE_PARTITION));
        m=dolly_allowed_modes(DOLLY_DEMO_COPY,1,
                              DOLLY_ENDPOINT_FILE,0);
        assert(m==DOLLY_MODE_BIT(DOLLY_MODE_FILE));
        m=dolly_allowed_modes(DOLLY_DEMO_COPY,1,
                              DOLLY_ENDPOINT_NONE,0);
        assert(m==0);
        m=dolly_allowed_modes(DOLLY_DEMO_CREATE_IMAGE,1,
                              DOLLY_ENDPOINT_LOGICAL,0);
        assert(m==DOLLY_MODE_BIT(DOLLY_MODE_FILE));
        m=dolly_allowed_modes(DOLLY_DEMO_DISK_VET,0,
                              DOLLY_ENDPOINT_NONE,0);
        assert(dolly_mode_allowed(m,DOLLY_MODE_PARTITION));
        assert(dolly_allowed_modes(DOLLY_DEMO_DISK_VET,1,
                                   DOLLY_ENDPOINT_PARTITION,0)==0);
        assert(!dolly_demo_needs_target(DOLLY_DEMO_DISK_VET));
        assert(!dolly_mode_allowed(0,(DollyEndpointMode)4));
    }
    assert(dolly_endpoint_set_partition(&src,0,0,0));
    assert(!dolly_demo_can_start(DOLLY_DEMO_DISK_VET,&src,&dst));
    assert(dolly_endpoint_set_file(&src,"RAM:from.hdf"));
    assert(dolly_endpoint_set_file(&dst,"RAM:to.hdf"));
    assert(dolly_demo_can_start(DOLLY_DEMO_COPY,&src,&dst));
    assert(!dolly_demo_can_start(DOLLY_DEMO_CREATE_IMAGE,&src,&dst));
    assert(!dolly_demo_can_start(DOLLY_DEMO_RESTORE,&src,&dst));
    puts("endpoint selection: PASS");
    return 0;
}
