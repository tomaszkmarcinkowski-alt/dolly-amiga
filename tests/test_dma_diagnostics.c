#include "dolly_dma_diagnostics.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void){
    DollyDeviceProfile db[1]={
       {{"LabController","lab.device",42,2},
        0x00fffffeu,2,0x1fe00u,1,
        "Lab note 7, not a real hardware certification",
        "Lab note documents DMA address visibility of this test fixture"}
    };
    DollyDeviceIdentity actual={"LabController","lab.device",42,2};
    DollyDmaDiagnostic d=dolly_dma_diagnostic_build(
        DOLLY_DMA_OUTSIDE_MASK,DOLLY_DMA_ADVICE_EN,db,1,&actual,1);
    assert(d.generic.operation_blocked);
    assert(d.has_documented_device_note);
    assert(strstr(d.device.known_behavior,"Lab note")!=0);
    assert(d.device.reference && !d.transfer_authorized);
    d=dolly_dma_diagnostic_build(DOLLY_DMA_OUTSIDE_MASK,
                                 DOLLY_DMA_ADVICE_EN,db,1,&actual,0);
    assert(!d.has_documented_device_note && !d.transfer_authorized);
    actual.driver_version=43;
    d=dolly_dma_diagnostic_build(DOLLY_DMA_OUTSIDE_MASK,
                                 DOLLY_DMA_ADVICE_EN,db,1,&actual,1);
    assert(!d.has_documented_device_note && !d.transfer_authorized);
    puts("DMA source-aware troubleshooting and safe fallback tests passed");
    return 0;
}
