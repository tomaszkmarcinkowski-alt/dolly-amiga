#ifndef DOLLY_AMIGA_LOG_VIEW_H
#define DOLLY_AMIGA_LOG_VIEW_H
#include "dolly_amiga_surface_scan.h"
struct Screen;

int dolly_amiga_log_view_open(struct Screen *screen,const DollySurfaceScan *scan,
                              const char *outcome);
void dolly_amiga_log_view_pump(void);
ULONG dolly_amiga_log_view_signal(void);
void dolly_amiga_log_view_close(void);
#endif
