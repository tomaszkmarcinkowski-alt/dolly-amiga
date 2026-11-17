#include "dolly_dma_diagnostics.h"
DollyDmaDiagnostic dolly_dma_diagnostic_build(
    DollyDmaStatus problem,DollyDmaAdviceLanguage language,
    const DollyDeviceProfile *profiles,uint32_t profile_count,
    const DollyDeviceIdentity *device,int database_verified){
    DollyDmaDiagnostic out={0};
    out.generic=dolly_dma_get_advice(problem,language);
    out.device=dolly_device_profile_dma_help(profiles,profile_count,device,
                                              problem,database_verified);
    out.has_documented_device_note=(uint8_t)
        (out.device.match==DOLLY_PROFILE_EXACT &&
         out.device.known_behavior!=0 &&
         out.device.reference!=0);
    /* Runtime DMA checks must still pass in the backend. */
    out.transfer_authorized=0;
    return out;
}
