#include "dolly_amiga_transfer_guard.h"
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
