#include "dolly_dma_advice.h"
#include <stddef.h>
typedef struct AdviceText {
    const char *code,*title,*reason,*step1,*step2,*step3;
} AdviceText;
static const AdviceText english[] = {
    {"DMA_OK","DMA checks completed",
     "Address-mask and alignment checks passed; controller compatibility is not proven.",
     "Continue only with other required safety checks.",NULL,NULL},
    {"DMA_PROFILE_UNKNOWN","DMA configuration not verified",
     "Dolly cannot establish safe DMA requirements for this controller.",
     "Identify the controller, its driver version and device unit.",
     "Check the controller manual for supported memory and DMA alignment.",
     "Use a documented, verified profile; do not guess Mask or MaxTransfer."},
    {"DMA_ADDRESS_MASK","DMA buffer outside allowed memory",
     "The selected buffer cannot be used with the current DMA address mask.",
     "Check which RAM types and address ranges the controller can access.",
     "Confirm whether documented 24-bit DMA-compatible RAM is required.",
     "Do not widen the mask or force a transfer to make the warning disappear."},
    {"DMA_ALIGNMENT","DMA buffer alignment is incompatible",
     "The allocated buffer does not meet the controller alignment requirement.",
     "Look up the controller's required alignment in its documentation.",
     "Confirm that the DMA-compatible allocator provides that alignment.",
     "Do not reduce the required alignment without hardware evidence."},
    {"DMA_ADDRESS_OVERFLOW","Invalid DMA address range",
     "The requested transfer exceeds a representable DMA address range.",
     "Check the device address and transfer length in the diagnostic log.",
     "Check the controller's supported addressing and transfer limits.",
     "Do not retry with truncated or wrapped addresses."},
    {"DMA_MEMORY_UNAVAILABLE","No suitable DMA memory",
     "Dolly could not reserve memory that satisfies the controller requirements.",
     "Close optional applications and check free compatible RAM.",
     "Check if the controller requires Chip RAM or documented 24-bit DMA RAM.",
     "Reduce workload only within documented limits; do not bypass DMA checks."}
};
static const AdviceText polish_ascii[] = {
    {"DMA_OK","Kontrola DMA zakonczona",
     "Maska adresowa i wyrownanie sa poprawne; nie potwierdza to zgodnosci kontrolera.",
     "Kontynuuj pozostale kontrole bezpieczenstwa.",NULL,NULL},
    {"DMA_PROFILE_UNKNOWN","Niezweryfikowana konfiguracja DMA",
     "Dolly nie zna bezpiecznych wymagan DMA tego kontrolera.",
     "Sprawdz model kontrolera, wersje sterownika i numer jednostki.",
     "Znajdz w dokumentacji obslugiwane typy pamieci i wyrownanie DMA.",
     "Uzyj sprawdzonego profilu; nie zgaduj wartosci Mask ani MaxTransfer."},
    {"DMA_ADDRESS_MASK","Bufor poza obszarem DMA",
     "Bufor nie miesci sie w obszarze adresowym dostepnym dla kontrolera.",
     "Sprawdz obslugiwane rodzaje RAM i zakres adresow kontrolera.",
     "Ustal, czy wymagany jest RAM zgodny z 24-bitowym DMA.",
     "Nie poszerzaj maski tylko po to, aby ominac ostrzezenie."},
    {"DMA_ALIGNMENT","Nieprawidlowe wyrownanie DMA",
     "Adres bufora nie spelnia wymagan wyrownania tego kontrolera.",
     "Sprawdz wymagane wyrownanie w instrukcji kontrolera.",
     "Zweryfikuj alokator pamieci zgodnej z DMA.",
     "Nie zmniejszaj wymagan wyrownania bez potwierdzenia."},
    {"DMA_ADDRESS_OVERFLOW","Niepoprawny zakres adresow DMA",
     "Zakres transferu nie miesci sie w prawidlowej przestrzeni adresowej.",
     "Sprawdz adres i dlugosc transferu w raporcie zaawansowanym.",
     "Zweryfikuj obslugiwane adresowanie i limity kontrolera.",
     "Nie ponawiaj operacji z obcietymi adresami."},
    {"DMA_MEMORY_UNAVAILABLE","Brak odpowiedniej pamieci DMA",
     "Dolly nie moze uzyskac pamieci zgodnej z wymaganiami kontrolera.",
     "Zamknij zbedne programy i sprawdz dostepny RAM.",
     "Sprawdz, czy wymagany jest Chip RAM lub pamiec zgodna z DMA 24-bit.",
     "Nie omijaj kontroli DMA w celu wymuszenia odczytu."}
};
DollyDmaAdvice dolly_dma_get_advice(DollyDmaStatus status,
                                   DollyDmaAdviceLanguage language){
    const AdviceText *catalog, *t;
    DollyDmaAdvice out;
    unsigned idx=(unsigned)status;
    if(language==DOLLY_DMA_ADVICE_PL_ASCII)catalog=polish_ascii;
    else catalog=english;
    if(idx>=sizeof(english)/sizeof(english[0]))idx=1; /* fail closed */
    t=&catalog[idx];
    out.code=t->code;out.title=t->title;out.reason=t->reason;
    out.action[0]=t->step1;out.action[1]=t->step2;out.action[2]=t->step3;
    out.action_count=t->step3?3u:t->step2?2u:t->step1?1u:0u;
    out.operation_blocked=(status!=DOLLY_DMA_OK);
    return out;
}
