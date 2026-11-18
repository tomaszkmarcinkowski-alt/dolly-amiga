#include "dolly_surface_log.h"
#include <stdio.h>
#include <stdlib.h>

void dolly_surface_log_add(DollySurfaceLog *log,DollySurfaceEventKind kind,
                           uint64_t lba,uint32_t sectors,DollyBlockStatus status) {
    DollySurfaceLogPage *page=log->last;
    DollySurfaceEvent *event;
    /* Keep a contiguous prefix and stop allocating if memory runs out. */
    if(log->omitted) {
        ++log->omitted;
        return;
    }
    if(!page || page->count==DOLLY_LOG_PAGE_SIZE) {
        page=calloc(1,sizeof(*page));
        if(!page) {
            log->omitted=1;
            return;
        }
        page->previous=log->last;
        if(log->last)log->last->next=page;
        else log->first=page;
        log->last=page;
    }
    event=&page->entries[page->count++];
    event->kind=kind;
    event->lba=lba;
    event->sectors=sectors;
    event->status=status;
}
void dolly_surface_log_clear(DollySurfaceLog *log) {
    DollySurfaceLogPage *page=log->first;
    while(page) {
        DollySurfaceLogPage *next=page->next;
        free(page);
        page=next;
    }
    log->first=log->last=NULL;
    log->omitted=0;
}
void dolly_surface_log_decimal(char out[21],uint64_t value) {
    char digits[20];
    unsigned count=0,i;
    do {
        digits[count++]=(char)('0'+value%10u);
        value/=10u;
    } while(value);
    for(i=0;i<count;++i)out[i]=digits[count-i-1u];
    out[count]='\0';
}
void dolly_surface_log_line(char line[DOLLY_LOG_LINE_SIZE],
                            const DollySurfaceEvent *event,const DollyMediaGeometry *geometry) {
    static const char *const names[]={
        "Batch failed","Retry","Recovered","Unreadable","Scan aborted"
    };
    const char *name=names[event->kind];
    char lba[21],chs[40],range[32]="";
    uint64_t track,cylinder;
    dolly_surface_log_decimal(lba,event->lba);
    snprintf(chs,sizeof(chs),"unavailable");
    if(geometry->cylinders && geometry->heads && geometry->sectors_per_track &&
       event->lba<geometry->block_count) {
        track=event->lba/geometry->sectors_per_track;
        cylinder=track/geometry->heads;
        if(cylinder<geometry->cylinders)
            snprintf(chs,sizeof(chs),"%lu/%lu/%lu",(unsigned long)cylinder,
                     (unsigned long)(track%geometry->heads),
                     (unsigned long)(event->lba%geometry->sectors_per_track+1u));
    }
    if(event->sectors!=1u)
        snprintf(range,sizeof(range)," +%lu sectors",(unsigned long)event->sectors);
    snprintf(line,DOLLY_LOG_LINE_SIZE,"LBA %s%s  CHS %s  %s (%s)",lba,range,chs,
             name,dolly_block_status_name(event->status));
}
static void geometry_number(char out[21],uint32_t value) {
    if(value)dolly_surface_log_decimal(out,value);
    else snprintf(out,21,"unavailable");
}
void dolly_surface_log_geometry(char line[DOLLY_LOG_LINE_SIZE],
                                const DollyMediaGeometry *geometry) {
    char cylinders[21],heads[21],spt[21];
    geometry_number(cylinders,geometry->cylinders);
    geometry_number(heads,geometry->heads);
    geometry_number(spt,geometry->sectors_per_track);
    snprintf(line,DOLLY_LOG_LINE_SIZE,"Geometry: %s cylinders / %s heads / %s sectors per track",
             cylinders,heads,spt);
}
