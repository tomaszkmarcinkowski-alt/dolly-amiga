#include "dolly_device_profiles.h"
#include <stddef.h>
#include <string.h>
static int identical(const DollyDeviceIdentity *a,const DollyDeviceIdentity *b){
    return a->controller_model && b->controller_model &&
           a->driver_name && b->driver_name &&
           strcmp(a->controller_model,b->controller_model)==0 &&
           strcmp(a->driver_name,b->driver_name)==0 &&
           a->driver_version==b->driver_version &&
           a->hardware_revision==b->hardware_revision;
}
DollyProfileSuggestion dolly_device_profile_suggest(
    const DollyDeviceProfile *profiles,uint32_t count,
    const DollyDeviceIdentity *device){
    DollyProfileSuggestion result={0};
    uint32_t i;
    if(!device||(!profiles&&count)||count>1024u)return result;
    for(i=0;i<count;i++){
        if(!identical(&profiles[i].identity,device))continue;
        result.candidate_count++;
        if(result.candidate_count==1u)result.profile=&profiles[i];
    }
    if(result.candidate_count==1u)result.match=DOLLY_PROFILE_EXACT;
    else if(result.candidate_count>1u){
        result.match=DOLLY_PROFILE_AMBIGUOUS;
        result.profile=NULL;
    }
    /* Even a unique exact match never authorizes the raw device adapter.
     * A verified controller profile requires independent runtime checks. */
    result.settings_may_be_activated=0;
    return result;
}

/* The exact-match record is a contextual advisory, not a DMA whitelist.
 * A file/version integrity failure prevents its use entirely. */
DollyProfileDmaHelp dolly_device_profile_dma_help(
    const DollyDeviceProfile *profiles,uint32_t count,
    const DollyDeviceIdentity *device,DollyDmaStatus problem,
    int database_compatible_and_verified){
    DollyProfileDmaHelp out={0};
    DollyProfileSuggestion suggestion;
    out.problem=problem;
    if(!database_compatible_and_verified || problem==DOLLY_DMA_OK)
        return out;
    suggestion=dolly_device_profile_suggest(profiles,count,device);
    out.match=suggestion.match;
    if(suggestion.match!=DOLLY_PROFILE_EXACT || !suggestion.profile)
        return out;
    if(!suggestion.profile->source_reference ||
       !suggestion.profile->source_reference[0] ||
       !suggestion.profile->documented_dma_behavior ||
       !suggestion.profile->documented_dma_behavior[0]){
        out.match=DOLLY_PROFILE_NONE;
        return out;
    }
    out.known_behavior=suggestion.profile->documented_dma_behavior;
    out.reference=suggestion.profile->source_reference;
    /* Do not enable raw I/O based on a text profile alone. */
    out.configuration_authorized=0;
    return out;
}
