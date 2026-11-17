#ifndef DOLLY_AMIGA_ENDPOINT_PICKER_H
#define DOLLY_AMIGA_ENDPOINT_PICKER_H
#include "dolly_endpoint.h"
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <exec/lists.h>
typedef struct DollyEndpointPicker {
    struct Screen *screen;
    struct Window *window;
    APTR visual;
    struct Gadget *gadgets;
    struct Gadget *device_list, *unit_list, *partition_list;
    struct List devices,units,partitions;
    struct Node device_nodes[2],unit_nodes[2],partition_nodes[2];
    DollyEndpoint result;
    unsigned device,unit,partition;
    unsigned has_device,has_unit,has_partition;
    unsigned role;
    unsigned char changed;
    char message[64];
} DollyEndpointPicker;
/* role: 0 source, 1 target. Two distinct, identical selectors. */
int dolly_picker_open(DollyEndpointPicker *picker,struct Screen *screen,
                      unsigned role,const DollyEndpoint *initial);
void dolly_picker_pump(DollyEndpointPicker *picker);
void dolly_picker_close(DollyEndpointPicker *picker);
#endif
