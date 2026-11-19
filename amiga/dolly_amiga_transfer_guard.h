#ifndef DOLLY_AMIGA_TRANSFER_GUARD_H
#define DOLLY_AMIGA_TRANSFER_GUARD_H
#include "dolly_amiga_physical_catalog.h"
struct Window;
/* Use the lock of the actual image file or its destination drawer. */
const char *dolly_amiga_guard_file_transfer(struct Window *parent,BPTR file_lock,
                         const DollyPhysicalEntry *raw_device,
                         uint64_t raw_start,uint64_t raw_length);
void dolly_amiga_warn_same_unit(struct Window *parent);
#endif
