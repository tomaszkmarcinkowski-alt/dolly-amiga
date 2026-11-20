#include "dolly_amiga_transfer_guard.h"
#include "dolly_amiga_destructive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dolly_amiga_io_settings.h"
#include "dolly_endpoint.h"
#include <strings.h>

int dolly_amiga_warn_same_unit(DollyWarnings *warnings) {
    return dolly_amiga_suppress_same_unit_warning() ||
           dolly_warning_add(warnings,"Same unit",
             "Source and destination are on the same device unit.\n"
             "Copying between them may be slower.","OK");
}

const char *dolly_amiga_guard_file_transfer(DollyWarnings *warnings,BPTR file_lock,
                         const DollyPhysicalEntry *raw_device,
                         uint64_t raw_start,uint64_t raw_length) {
    DollyPhysicalEntry file_device;
    uint64_t first,length;
    DollyUnitRelation relation;
    int kind=dolly_physical_from_lock(file_lock,&file_device,&first,&length);
    if(kind==2)return NULL;
    if(kind!=1)
        return dolly_warning_add(warnings,"Unknown image volume",
            "Dolly could not identify the image volume's device.\n"
            "Continuing could overwrite data on the source volume. Proceed at your own risk.",
            "Proceed")?NULL:"Cannot collect image-volume warning.";
    if(raw_device->unit!=file_device.unit ||
       strcasecmp(raw_device->driver,file_device.driver)!=0)return NULL;
    relation=dolly_unit_relation(raw_start,raw_length,first,length);
    if(relation==DOLLY_UNIT_RANGE_OVERLAP)
        return "The image volume overlaps the selected raw range.";
    if(relation!=DOLLY_UNIT_RANGE_DISJOINT)
        return "Dolly cannot verify that the ranges on this unit are separate.";
    return dolly_amiga_warn_same_unit(warnings)?NULL:"Cannot collect same-unit warning.";
}

struct DollyReadGuard {
    DollyPhysicalEntry device;
    uint64_t start,length;
    DollyMountList pending,live;
    struct Window *parent;
};

static const DollyMountedVolume *read_mount_match(const DollyMountList *list,
                                                    const DollyMountedVolume *mount) {
    unsigned i;
    for(i=0;i<list->count;++i) {
        const DollyMountedVolume *candidate=&list->entries[i];
        if(candidate->entry==mount->entry && candidate->handler==mount->handler &&
           !strcasecmp(candidate->device,mount->device))return candidate;
    }
    return NULL;
}

static int inhibit_source_warning(DollyWarning *warning) {
    DollyReadGuard *guard=warning->context;
    DollyMountedVolume *mount=&guard->pending.entries[warning->index];
    const DollyMountedVolume *actual;
    char reason[240];
    if(!dolly_amiga_mounted_extents(&guard->device,guard->start,guard->length,0,&guard->live))
        return -1;
    actual=read_mount_match(&guard->live,mount);
    if(!actual || strcmp(actual->volume,mount->volume)) {
        dolly_amiga_role_request(guard->parent,"Dolly - Source changed",
                                 "The source filesystem changed. Start again.","OK",NULL,NULL);
        return -1;
    }
    mount->suspended=dolly_amiga_inhibit_mount(mount,1);
    if(!dolly_amiga_mounted_extents(&guard->device,guard->start,guard->length,0,&guard->live))
        return -1;
    actual=read_mount_match(&guard->live,mount);
    if(mount->suspended && (!actual || !actual->volume[0]))return 1;
    snprintf(reason,sizeof(reason),"Source: \"%s\"\nVolume: \"%s\"\n"
            "Dolly could not verify Inhibit. Other tasks can still change the source. "
            "Proceed at your own risk.",mount->device,
            mount->volume[0]?mount->volume:"name unavailable");
    mount->allow_present=1;
    return dolly_warning_risk(warning,reason);
}

int dolly_amiga_guard_read(DollyReadGuard **guard,struct Window *parent,DollyWarnings *warnings,
                           const DollyPhysicalEntry *device,uint64_t start,uint64_t length,
                           BPTR required_file_lock) {
    DollyReadGuard *g;
    unsigned i;
    if(*guard || !length || start>UINT64_MAX-length)return 0;
    g=calloc(1,sizeof(*g));
    if(!g)return 0;
    *guard=g;
    g->device=*device;
    g->start=start;
    g->length=length;
    g->parent=parent;
    if(!dolly_amiga_mounted_extents(device,start,length,0,&g->pending))return 0;
    for(i=0;i<g->pending.count;++i) {
        DollyMountedVolume *mount=&g->pending.entries[i];
        DollyWarning *warning;
        char title[46],reason[240];
        if(dolly_amiga_lock_uses_mount(required_file_lock,mount)) {
            dolly_amiga_role_request(parent,"Dolly - Transfer blocked",
                "The file uses a filesystem that the raw operation must inhibit.",
                "OK",NULL,NULL);
            return 0;
        }
        snprintf(title,sizeof(title),"Source %.36s",mount->device);
        snprintf(reason,sizeof(reason),"Source: \"%s\"\nVolume: \"%s\"\n"
                 "Temporarily inhibit filesystem access while Dolly reads these sectors?",
                 mount->device,mount->volume[0]?mount->volume:"name unavailable");
        warning=dolly_warning_add(warnings,title,reason,"Inhibit");
        if(!warning)return 0;
        warning->action=inhibit_source_warning;
        warning->context=g;
        warning->index=i;
    }
    return 1;
}

int dolly_amiga_read_guard_isolated(DollyReadGuard *guard) {
    unsigned i;
    if(!dolly_amiga_read_guard_check(guard))return 0;
    if(guard)for(i=0;i<guard->pending.count;++i)
        if(!guard->pending.entries[i].suspended)return 0;
    return 1;
}

int dolly_amiga_read_guard_allows_lock(const DollyReadGuard *guard,BPTR lock) {
    unsigned i;
    if(guard)for(i=0;i<guard->pending.count;++i)
        if(dolly_amiga_lock_uses_mount(lock,&guard->pending.entries[i]))return 0;
    return 1;
}

int dolly_amiga_read_guard_check(DollyReadGuard *guard) {
    unsigned i;
    if(!guard)return 1;
    if(!dolly_amiga_mounted_extents(&guard->device,guard->start,guard->length,0,&guard->live))return 0;
    for(i=0;i<guard->live.count;++i) {
        const DollyMountedVolume *current=&guard->live.entries[i];
        const DollyMountedVolume *prior=read_mount_match(&guard->pending,current);
        if(!prior || (!prior->suspended && !prior->allow_present) ||
           (current->volume[0] &&
            (!prior->allow_present || strcmp(current->volume,prior->volume))))return 0;
    }
    return 1;
}

void dolly_amiga_read_guard_close(DollyReadGuard **guard) {
    DollyReadGuard *g=*guard;
    unsigned i;
    if(!g)return;
    for(i=0;i<g->pending.count;++i) {
        const DollyMountedVolume *mount=&g->pending.entries[i];
        if(mount->suspended && !dolly_amiga_inhibit_mount(mount,0)) {
            char reason[120];
            snprintf(reason,sizeof(reason),"Could not resume access to \"%s\".",mount->device);
            dolly_amiga_role_request(g->parent,"Dolly - Filesystem inhibited",reason,"OK",NULL,NULL);
        }
    }
    free(g);
    *guard=NULL;
}
