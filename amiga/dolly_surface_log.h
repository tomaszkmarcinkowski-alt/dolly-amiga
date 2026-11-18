#ifndef DOLLY_SURFACE_LOG_H
#define DOLLY_SURFACE_LOG_H

#include "dolly_block_source.h"
#include "dolly_media_geometry.h"

#define DOLLY_LOG_PAGE_SIZE 24u
#define DOLLY_LOG_LINE_SIZE 160u

typedef enum DollySurfaceEventKind {
    DOLLY_SURFACE_BATCH_FAILED,
    DOLLY_SURFACE_RETRY,
    DOLLY_SURFACE_RECOVERED,
    DOLLY_SURFACE_UNREADABLE,
    DOLLY_SURFACE_ABORTED
} DollySurfaceEventKind;

typedef struct DollySurfaceEvent {
    uint64_t lba;
    uint32_t sectors;
    DollySurfaceEventKind kind;
    DollyBlockStatus status;
} DollySurfaceEvent;

typedef struct DollySurfaceLogPage {
    struct DollySurfaceLogPage *previous,*next;
    unsigned count;
    DollySurfaceEvent entries[DOLLY_LOG_PAGE_SIZE];
} DollySurfaceLogPage;

typedef struct DollySurfaceLog {
    DollySurfaceLogPage *first,*last;
    uint64_t omitted;
} DollySurfaceLog;

void dolly_surface_log_add(DollySurfaceLog *log,DollySurfaceEventKind kind,
                           uint64_t lba,uint32_t sectors,DollyBlockStatus status);
void dolly_surface_log_clear(DollySurfaceLog *log);
void dolly_surface_log_line(char line[DOLLY_LOG_LINE_SIZE],
                            const DollySurfaceEvent *event,const DollyMediaGeometry *geometry);
void dolly_surface_log_geometry(char line[DOLLY_LOG_LINE_SIZE],
                                const DollyMediaGeometry *geometry);
void dolly_surface_log_decimal(char out[21],uint64_t value);

#endif
