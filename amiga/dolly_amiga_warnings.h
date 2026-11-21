#ifndef DOLLY_AMIGA_WARNINGS_H
#define DOLLY_AMIGA_WARNINGS_H
#include "dolly_endpoint.h"
#include <exec/nodes.h>

struct Window;
typedef struct DollyWarning DollyWarning;
/* 1 acknowledges the item, 0 shows its updated details, -1 cancels review.
 * An action can append another item to the active review before returning 1. */
typedef int (*DollyWarningAction)(DollyWarning *warning);
struct DollyWarning {
    struct Node node;
    char row[48];
    char *text;
    const char *button;
    DollyWarningAction action;
    void *context;
    unsigned index,delay_seconds;
    const DollyEndpoint *settings;
    int confirmed,can_keep,kept;
};
#define DOLLY_WARNING_MAX 40u
typedef struct DollyWarnings {
    DollyWarning *items[DOLLY_WARNING_MAX];
    unsigned count;
    const DollyWarning *mount_summary;
    int *mount_destination; /* Optional. Updated only on approval. */
} DollyWarnings;
DollyWarning *dolly_warning_add(DollyWarnings *warnings,const char *title,
                               const char *details,const char *button);
int dolly_warnings_review(struct Window *parent,DollyWarnings *warnings);
void dolly_warnings_clear(DollyWarnings *warnings);
#endif
