#include "dolly_device_profiles.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    DollyDeviceProfile profiles[2]={
        {{"ExampleIDE","ide.device",40,1},0x00fffffeu,2,0x1fe00u,1,
         "example-only: not validated on physical hardware",
         "Documented 24-bit DMA memory limitation; see referenced controller documentation"},
        {{"ExampleIDE","ide.device",41,1},0xffffffffu,4,0x10000u,0,
         "example-only: not validated on physical hardware",
         "Controller-specific alignment must be confirmed by documentation"}
    };
    DollyDeviceIdentity id={"ExampleIDE","ide.device",40,1};
    DollyProfileSuggestion r=dolly_device_profile_suggest(profiles,2,&id);
    assert(r.match==DOLLY_PROFILE_EXACT);
    assert(r.profile==&profiles[0]);
    assert(r.candidate_count==1);
    assert(r.settings_may_be_activated==0);
    {
        DollyProfileDmaHelp h=dolly_device_profile_dma_help(profiles,2,&id,
                                       DOLLY_DMA_OUTSIDE_MASK,1);
        assert(h.match==DOLLY_PROFILE_EXACT);
        assert(h.known_behavior&&h.reference);
        assert(h.configuration_authorized==0);
        h=dolly_device_profile_dma_help(profiles,2,&id,DOLLY_DMA_OUTSIDE_MASK,0);
        assert(h.match==DOLLY_PROFILE_NONE&&!h.known_behavior);
        h=dolly_device_profile_dma_help(profiles,2,&id,DOLLY_DMA_OK,1);
        assert(h.match==DOLLY_PROFILE_NONE&&!h.known_behavior);
    }
    id.driver_version=42;
    r=dolly_device_profile_suggest(profiles,2,&id);
    assert(r.match==DOLLY_PROFILE_NONE && r.profile==NULL);
    id.driver_version=40;
    profiles[1].identity.driver_version=40;
    r=dolly_device_profile_suggest(profiles,2,&id);
    assert(r.match==DOLLY_PROFILE_AMBIGUOUS && r.profile==NULL);
    {
        DollyProfileDmaHelp h=dolly_device_profile_dma_help(profiles,2,&id,
                                     DOLLY_DMA_MISALIGNED,1);
        assert(h.match==DOLLY_PROFILE_AMBIGUOUS&&!h.known_behavior);
        assert(h.configuration_authorized==0);
    }
    assert(r.settings_may_be_activated==0);
    r=dolly_device_profile_suggest(NULL,1,&id);
    assert(r.match==DOLLY_PROFILE_NONE && r.settings_may_be_activated==0);
    puts("Device profile suggestions never authorize unsafe DMA tests passed");
    return 0;
}
