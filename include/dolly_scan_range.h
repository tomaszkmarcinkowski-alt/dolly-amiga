#ifndef DOLLY_SCAN_RANGE_H
#define DOLLY_SCAN_RANGE_H
#include "dolly_media_geometry.h"
/* Absolute decimal LBA or C/H/S (zero-based C/H, one-based S). */
int dolly_scan_position(const char *text,const DollyMediaGeometry *geometry,uint64_t *lba);
#endif
