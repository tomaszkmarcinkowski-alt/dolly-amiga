#include "dolly_amiga_transfer_guard.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_io_settings.h"
#include "dolly_endpoint.h"
#include <intuition/intuition.h>
#include <proto/intuition.h>
#include <strings.h>

void dolly_amiga_warn_same_unit(struct Window *parent) {
    if(parent && !dolly_amiga_suppress_same_unit_warning()) {
        struct EasyStruct request={
            sizeof(struct EasyStruct),0,(STRPTR)"Dolly - Copy performance",
            (STRPTR)"Source and destination are on the same device unit.\n"
                    "Copying between them may be slower.",
            (STRPTR)"OK"
        };
        EasyRequestArgs(parent,&request,NULL,NULL);
    }
}

const char *dolly_amiga_guard_file_transfer(struct Window *parent,BPTR file_lock,
                         const DollyPhysicalEntry *raw_device,
                         uint64_t raw_start,uint64_t raw_length) {
    DollyPhysicalEntry file_device;
    uint64_t first,length;
    DollyUnitRelation relation;
    int kind=dolly_physical_from_lock(file_lock,&file_device,&first,&length);
    if(kind==2)return NULL;
    if(kind!=1) {
        if(dolly_amiga_confirm_destructive(parent,
                "Dolly could not identify the image volume's device.\n"
                "Continuing could overwrite data on the source volume."))
            return NULL;
        return "Transfer cancelled. Image volume device is unknown.";
    }
    if(raw_device->unit!=file_device.unit ||
       strcasecmp(raw_device->driver,file_device.driver)!=0)return NULL;
    relation=dolly_unit_relation(raw_start,raw_length,first,length);
    if(relation!=DOLLY_UNIT_RANGE_DISJOINT) {
        const char *reason=relation==DOLLY_UNIT_RANGE_OVERLAP?
            "The image volume overlaps the selected raw range.":
            "Dolly cannot verify that the ranges on this unit are separate.";
        struct EasyStruct request={
            sizeof(struct EasyStruct),0,(STRPTR)"Dolly - Transfer blocked",
            (STRPTR)reason,(STRPTR)"OK"
        };
        if(parent)EasyRequestArgs(parent,&request,NULL,NULL);
        return reason;
    }
    dolly_amiga_warn_same_unit(parent);
    return NULL;
}
