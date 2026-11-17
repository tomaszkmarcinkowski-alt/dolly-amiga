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
    struct Gadget *logical_list,*choose_button,*file_text;
    struct Gadget *format_cycle,*file_info,*selection_text;
    struct List devices,units,partitions,logical_devices;
    struct Node device_nodes[2],unit_nodes[2],partition_nodes[2];
    struct Node logical_nodes[2];
    DollyEndpoint result;
    DollyEndpoint pending_file;
    char file_display[256];
    char file_details[160];
    char selection_display[160];
    DollyImageFormat output_format;
    unsigned device,unit,partition,logical;
    DollyEndpointMode mode;
    unsigned has_device,has_unit,has_partition,has_logical;
    unsigned role;
    unsigned char changed;
    unsigned char gadgets_detached;
    UWORD last_click_id,last_click_code;
    ULONG last_click_seconds,last_click_micros;
    unsigned char last_click_valid;
    char message[64];
} DollyEndpointPicker;
/* role: 0 source, 1 target. Two distinct, identical selectors. */
int dolly_picker_open(DollyEndpointPicker *picker,struct Screen *screen,
                      unsigned role,const DollyEndpoint *initial,
                      DollyEndpointMode preferred_mode);
void dolly_picker_pump(DollyEndpointPicker *picker);
void dolly_picker_close(DollyEndpointPicker *picker);
#endif
