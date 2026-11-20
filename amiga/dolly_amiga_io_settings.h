#ifndef DOLLY_AMIGA_IO_SETTINGS_H
#define DOLLY_AMIGA_IO_SETTINGS_H
#include "dolly_amiga_physical_catalog.h"
#include "dolly_endpoint.h"
#define DOLLY_PREFS_MAX 64u
struct Screen;
DollyIoHints dolly_amiga_io_override(const DollyPhysicalEntry *device);
int dolly_amiga_io_store(const DollyPhysicalEntry *device,DollyIoHints hints);
int dolly_amiga_io_entry(unsigned index,DollyPhysicalEntry *device,DollyIoHints *hints);
int dolly_amiga_legacy_override(void);
void dolly_amiga_set_legacy_override(int enabled);
int dolly_amiga_verify_writes(void);
void dolly_amiga_set_verify_writes(int enabled);
int dolly_amiga_suppress_same_unit_warning(void);
void dolly_amiga_set_suppress_same_unit_warning(int enabled);
int dolly_amiga_mount_destination_default(void);
void dolly_amiga_set_mount_destination_default(int enabled);
int dolly_amiga_allow_outside_range(void);
void dolly_amiga_set_allow_outside_range(int enabled);
void dolly_amiga_io_settings(struct Screen *screen,const DollyEndpoint *endpoint);
void dolly_amiga_io_settings_clear(void);
const char *dolly_amiga_prefs_load(int argc,char **argv);
int dolly_amiga_prefs_save(void);
#endif
