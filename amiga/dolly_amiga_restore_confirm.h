#ifndef DOLLY_AMIGA_RESTORE_CONFIRM_H
#define DOLLY_AMIGA_RESTORE_CONFIRM_H
#include "dolly_restore_check.h"
#include "dolly_amiga_restore_source.h"
struct Window;
int dolly_amiga_confirm_restore_assessment(struct Window *parent,
                                           const DollyRestoreAssessment *assessment);
int dolly_amiga_restore_preflight(struct Window *parent,
                       const DollyAmigaRestoreSource *image,
                       const DollyRestoreExtent *target);
void dolly_amiga_restore_preview(struct Window *parent,const DollyEndpoint *image,
                                const DollyEndpoint *destination);
#endif
