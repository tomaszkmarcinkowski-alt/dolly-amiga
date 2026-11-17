#include "dolly_profile_config.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    DollyDeviceProfile db[1]={
        {{"DemoBoard","demo.device",40,2},0x00fffffeu,2,8192u,1,
         "Synthetic evidence only, not a vendor-certified profile",
         "Test fixture notes a smaller transfer chunk"}
    };
    DollyDeviceIdentity id={"DemoBoard","demo.device",40,2};
    DollyLocalIoSettings current={512,65536,3};
    DollyConfigProposal p=dolly_profile_prepare_config(db,1,&id,1,current);
    assert(p.status==DOLLY_CONFIG_PROPOSAL_READY);
    assert(p.before.transfer_limit_bytes==65536u);
    assert(p.after.transfer_limit_bytes==8192u);
    assert(p.after.retry_limit==3);
    assert(p.requires_confirmation==1 && p.has_dma_authorization==0);
    assert(p.source_reference!=0);
    p=dolly_profile_prepare_config(db,1,&id,0,current);
    assert(p.status==DOLLY_CONFIG_REJECTED && !p.has_dma_authorization);
    db[0].proposed_max_transfer=131072u;
    p=dolly_profile_prepare_config(db,1,&id,1,current);
    assert(p.status==DOLLY_CONFIG_NO_CHANGE);
    db[0].proposed_max_transfer=8193u;
    p=dolly_profile_prepare_config(db,1,&id,1,current);
    assert(p.status==DOLLY_CONFIG_NO_CHANGE);
    db[0].proposed_max_transfer=8192u;
    id.driver_version=41;
    p=dolly_profile_prepare_config(db,1,&id,1,current);
    assert(p.status==DOLLY_CONFIG_NO_CHANGE);
    puts("Profile-based local safe settings proposals passed");
    return 0;
}
