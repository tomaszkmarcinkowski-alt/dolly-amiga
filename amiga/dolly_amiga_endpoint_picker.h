#ifndef DOLLY_AMIGA_ENDPOINT_PICKER_H
#define DOLLY_AMIGA_ENDPOINT_PICKER_H
#include "dolly_endpoint.h"
#include "dolly_amiga_dos_devices.h"
#include "dolly_amiga_physical_catalog.h"
#include "dolly_amiga_partition_scan.h"
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
    struct Gadget *format_cycle,*file_info,*selection_text,*disk_info_text;
    struct Gadget *disk_size_text,*query_button,*partition_info_text;
    struct List devices,units,logical_devices;
    /* Query builds an inactive bank and then swaps the LISTVIEW
     * pointer. Never modify the nodes/strings of its active bank.
     */
    struct List partition_lists[2];
    struct Node device_nodes[DOLLY_PHYS_MAX],unit_nodes[DOLLY_PHYS_MAX];
    struct Node partition_nodes[2][DOLLY_PART_MAX];
    char partition_list_labels[2][DOLLY_PART_MAX][48];
    unsigned partition_bank;
    DollyNativePartitionMap partition_map;
    DollyMediaGeometry media_geometry;
    unsigned partition_count;
    unsigned char partition_scanned;
    char physical_names[DOLLY_PHYS_MAX][DOLLY_PHYS_DRIVER_LEN];
    char unit_names[DOLLY_PHYS_MAX][20];
    ULONG physical_units[DOLLY_PHYS_MAX];
    unsigned device_count,unit_count;
    unsigned char queried;
    unsigned char query_attempted;
    DollyPhysicalCatalog physical_catalog;
    struct Node logical_nodes[DOLLY_DOS_MAX_DEVICES];
    DollyDosDevices dos_devices;
    DollyEndpoint result;
    DollyEndpoint pending_file;
    char file_display[256];
    char file_details[160];
    char selection_display[160];
    char disk_info_display[160];
    char disk_size_display[40];
    char partition_info_display[160];
    DollyImageFormat output_format;
    unsigned device,unit,partition,logical;
    DollyEndpointMode mode;
    unsigned has_device,has_unit,has_partition,has_logical;
    unsigned logical_count;
    unsigned role;
    unsigned char changed;
    unsigned char gadgets_detached;
    char message[64];
} DollyEndpointPicker;
/* role: 0 source, 1 target. Two distinct, identical selectors. */
int dolly_picker_open(DollyEndpointPicker *picker,struct Screen *screen,
                      unsigned role,const DollyEndpoint *initial,
                      DollyEndpointMode preferred_mode);
void dolly_picker_pump(DollyEndpointPicker *picker);
void dolly_picker_close(DollyEndpointPicker *picker);
#endif
