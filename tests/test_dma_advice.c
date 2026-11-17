#include "dolly_dma_advice.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void check_language(DollyDmaAdviceLanguage language){
    unsigned i;
    for(i=0;i<=DOLLY_DMA_NO_COMPATIBLE_MEMORY;i++){
        unsigned j;
        DollyDmaAdvice a=dolly_dma_get_advice((DollyDmaStatus)i,language);
        assert(a.code&&a.title&&a.reason);
        assert(strlen(a.code)>=6&&strlen(a.title)>6&&strlen(a.reason)>16);
        assert(a.action_count>=1&&a.action_count<=3);
        assert(a.operation_blocked==(i!=DOLLY_DMA_OK));
        for(j=0;j<a.action_count;j++)assert(a.action[j]&&strlen(a.action[j])>18);
        for(j=a.action_count;j<3;j++)assert(!a.action[j]);
    }
}
int main(void){
    DollyDmaAdvice a;
    check_language(DOLLY_DMA_ADVICE_EN);
    check_language(DOLLY_DMA_ADVICE_PL_ASCII);
    a=dolly_dma_get_advice(DOLLY_DMA_BAD_PROFILE,DOLLY_DMA_ADVICE_EN);
    assert(!strcmp(a.code,"DMA_PROFILE_UNKNOWN"));
    assert(strstr(a.action[2],"do not guess")!=NULL);
    a=dolly_dma_get_advice(DOLLY_DMA_OUTSIDE_MASK,DOLLY_DMA_ADVICE_PL_ASCII);
    assert(!strcmp(a.code,"DMA_ADDRESS_MASK"));
    assert(strstr(a.action[2],"Nie poszerzaj")!=NULL);
    a=dolly_dma_get_advice((DollyDmaStatus)999,DOLLY_DMA_ADVICE_EN);
    assert(a.operation_blocked&&!strcmp(a.code,"DMA_PROFILE_UNKNOWN"));
    puts("Structured DMA guidance test passed");
    return 0;
}
