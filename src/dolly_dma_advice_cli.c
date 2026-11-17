/* Standalone, read-only DMA problem help for host and classic AmigaOS. */
#include "dolly_dma_advice.h"
#include <stdio.h>
#include <string.h>
static int decode(const char *s,DollyDmaStatus *status){
    unsigned i;
    for(i=0;i<=DOLLY_DMA_NO_COMPATIBLE_MEMORY;i++){
        DollyDmaAdvice a=dolly_dma_get_advice((DollyDmaStatus)i,DOLLY_DMA_ADVICE_EN);
        if(strcmp(s,a.code)==0){*status=(DollyDmaStatus)i;return 1;}
    }
    return 0;
}
int main(int argc,char **argv){
    DollyDmaStatus status;
    DollyDmaAdvice a;
    DollyDmaAdviceLanguage lang=DOLLY_DMA_ADVICE_EN;
    unsigned i;
    if(argc<2 || argc>3 || !decode(argv[1],&status) ||
       (argc==3 && strcmp(argv[2],"pl") && strcmp(argv[2],"en"))){
        puts("Usage: dolly-dma-advice DMA_CODE [en|pl]");
        puts("Codes: DMA_PROFILE_UNKNOWN DMA_ADDRESS_MASK DMA_ALIGNMENT");
        puts("       DMA_ADDRESS_OVERFLOW DMA_MEMORY_UNAVAILABLE DMA_OK");
        return 20;
    }
    if(argc==3 && !strcmp(argv[2],"pl"))lang=DOLLY_DMA_ADVICE_PL_ASCII;
    a=dolly_dma_get_advice(status,lang);
    printf("[%s] %s\n%s\n",a.code,a.title,a.reason);
    if(a.operation_blocked)
        puts(lang==DOLLY_DMA_ADVICE_PL_ASCII?
             "Operacja zatrzymana. Brak automatycznych zmian ustawien.":
             "Operation stopped. No settings were changed automatically.");
    for(i=0;i<a.action_count;i++)printf("%u. %s\n",i+1,a.action[i]);
    return status==DOLLY_DMA_OK?0:10; /* exit code reflects blocking problem */
}
