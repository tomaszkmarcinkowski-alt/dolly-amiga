#include "dolly_device_profiles.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    DollyDeviceProfile profiles[2]={
        {{"ExampleIDE","ide.device",40,1},0x00fffffeu,2,0x1fe00u,1,
         "example-only: not validated on physical hardware"},
        {{"ExampleIDE","ide.device",41,1},0xffffffffu,4,0x10000u,0,
         "example-only: not validated on physical hardware"}
    };
    DollyDeviceIdentity id={"ExampleIDE","ide.device",40,1};
    DollyProfileSuggestion r=dolly_device_profile_suggest(profiles,2,&id);
    assert(r.match==DOLLY_PROFILE_EXACT);
    assert(r.profile==&profiles[0]);
    assert(r.candidate_count==1);
    assert(r.settings_may_be_activated==0);
    id.driver_version=42;
    r=dolly_device_profile_suggest(profiles,2,&id);
    assert(r.match==DOLLY_PROFILE_NONE && r.profile==NULL);
    id.driver_version=40;
    profiles[1].identity.driver_version=40;
    r=dolly_device_profile_suggest(profiles,2,&id);
    assert(r.match==DOLLY_PROFILE_AMBIGUOUS && r.profile==NULL);
    assert(r.settings_may_be_activated==0);
    r=dolly_device_profile_suggest(NULL,1,&id);
    assert(r.match==DOLLY_PROFILE_NONE && r.settings_may_be_activated==0);
    puts("Device profile suggestions never authorize unsafe DMA tests passed");
    return 0;
}
