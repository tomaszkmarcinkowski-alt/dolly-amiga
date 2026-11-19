#ifndef DOLLY_AMIGA_DEVICE_QUERY_H
#define DOLLY_AMIGA_DEVICE_QUERY_H
#include <exec/types.h>
#include <stdint.h>
#include "dolly_media_geometry.h"
/* Parse a picker path and return its suffix after the actual unit number. */
const char *dolly_amiga_parse_unit(const char *path,char *driver,
                                  unsigned capacity,ULONG *unit);
int dolly_amiga_device_query(const char *driver,ULONG unit,
                            char *name,unsigned name_capacity,
                            char *size,unsigned size_capacity,
                            DollyMediaGeometry *geometry,int probe_chs,
                            char *ata,unsigned ata_capacity);
#endif
