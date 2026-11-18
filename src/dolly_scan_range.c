#include "dolly_scan_range.h"

static int decimal(const char **text,uint64_t *out) {
    const char *p=*text;
    uint64_t value=0;
    if(*p<'0' || *p>'9')return 0;
    do {
        unsigned digit=(unsigned)(*p-'0');
        if(value>(UINT64_MAX-digit)/10u)return 0;
        value=value*10u+digit;
        ++p;
    } while(*p>='0' && *p<='9');
    *out=value;
    *text=p;
    return 1;
}
int dolly_scan_position(const char *text,const DollyMediaGeometry *geometry,uint64_t *lba) {
    uint64_t cylinder,head,sector,track;
    if(!decimal(&text,&cylinder))return 0;
    if(!*text) {
        *lba=cylinder;
        return 1;
    }
    if(*text++!='/' || !decimal(&text,&head) || *text++!='/' ||
       !decimal(&text,&sector) || *text || cylinder>=geometry->cylinders ||
       head>=geometry->heads || !sector || sector>geometry->sectors_per_track)
        return 0;
    track=cylinder*geometry->heads+head;
    if(track>(UINT64_MAX-(sector-1u))/geometry->sectors_per_track)return 0;
    *lba=track*geometry->sectors_per_track+sector-1u;
    return 1;
}
