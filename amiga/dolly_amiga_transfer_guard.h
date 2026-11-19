#ifndef DOLLY_AMIGA_TRANSFER_GUARD_H
#define DOLLY_AMIGA_TRANSFER_GUARD_H
#include "dolly_amiga_physical_catalog.h"
#include "dolly_amiga_warnings.h"
/* Use the lock of the actual image file or its destination drawer. */
const char *dolly_amiga_guard_file_transfer(DollyWarnings *warnings,BPTR file_lock,
                         const DollyPhysicalEntry *raw_device,
                         uint64_t raw_start,uint64_t raw_length);
int dolly_amiga_warn_same_unit(DollyWarnings *warnings);
typedef struct DollyReadGuard DollyReadGuard;
int dolly_amiga_guard_read(DollyReadGuard **guard,struct Window *parent,DollyWarnings *warnings,
                           const DollyPhysicalEntry *device,uint64_t start,uint64_t length,
                           BPTR required_file_lock);
int dolly_amiga_read_guard_check(DollyReadGuard *guard);
void dolly_amiga_read_guard_close(DollyReadGuard **guard);
#endif
