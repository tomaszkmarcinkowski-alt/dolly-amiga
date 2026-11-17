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
    struct Gadget *mode_cycle,*device_list,*unit_list,*partition_list;
    struct Gadget *logical_list,*choose_button;
    struct List devices,units,partitions,logical_devices;
    struct Node device_nodes[2],unit_nodes[2],partition_nodes[2];
    struct Node logical_nodes[2];
    DollyEndpoint result;
    unsigned device,unit,partition,logical;
    DollyEndpointMode mode;
    unsigned has_device,has_unit,has_partition,has_logical;
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
