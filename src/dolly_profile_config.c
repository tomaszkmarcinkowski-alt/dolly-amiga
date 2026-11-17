#include "dolly_profile_config.h"
DollyConfigProposal dolly_profile_prepare_config(
    const DollyDeviceProfile *profiles,uint32_t count,
    const DollyDeviceIdentity *device,int database_verified,
    DollyLocalIoSettings current){
    DollyConfigProposal out={0};
    DollyProfileSuggestion match;
    uint32_t limit;
    out.before=current;out.after=current;
    if(!database_verified || !current.block_bytes ||
       current.block_bytes<512u || current.block_bytes>32768u ||
       (current.block_bytes&(current.block_bytes-1u)) ||
       !current.transfer_limit_bytes ||
       current.transfer_limit_bytes%current.block_bytes ||
       current.transfer_limit_bytes>65536u){
        out.status=DOLLY_CONFIG_REJECTED;return out;
    }
    match=dolly_device_profile_suggest(profiles,count,device);
    if(match.match!=DOLLY_PROFILE_EXACT || !match.profile ||
       !match.profile->source_reference ||
       !match.profile->source_reference[0] ||
       !match.profile->documented_dma_behavior ||
       !match.profile->documented_dma_behavior[0]){
        out.status=DOLLY_CONFIG_NO_CHANGE;return out;
    }
    limit=match.profile->proposed_max_transfer;
    /* The database can only suggest a SMALLER, aligned Dolly-local chunk.
     * Do not modify OS/RDB MaxTransfer, dma_profile_verified or masks. */
    if(limit<current.block_bytes || limit%current.block_bytes!=0 ||
       limit>=current.transfer_limit_bytes){
        out.status=DOLLY_CONFIG_NO_CHANGE;return out;
    }
    out.after.transfer_limit_bytes=limit;
    out.source_reference=match.profile->source_reference;
    out.requires_confirmation=1;
    out.status=DOLLY_CONFIG_PROPOSAL_READY;
    return out;
}
