#ifndef DOLLY_AMIGA_INVENTORY_VIEW_H
#define DOLLY_AMIGA_INVENTORY_VIEW_H
/* M1.45: dedicated, explicitly requested, read-only AmigaOS inventory.
 * No device write is performed. A device with unsupported geometry is
 * displayed as unverified, not silently treated as empty.
 */
#include "dolly_endpoint.h"
#include "dolly_amiga_partition_scan.h"
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <exec/lists.h>
#define DOLLY_INVENTORY_ROWS (DOLLY_PART_MAX + 1u)
#define DOLLY_INVENTORY_DETAILS 10u
#define DOLLY_INVENTORY_DETAIL_BYTES 96u
#define DOLLY_INVENTORY_HEADER_ROWS 4u
#define DOLLY_INVENTORY_HEADER_BYTES 176u
typedef struct DollyNativeInventoryView {
    struct Window *window;
    APTR visual;
    struct Gadget *gadgets;
    struct Menu *menu;
    struct List rows;
    struct Node nodes[DOLLY_INVENTORY_ROWS];
    char row_text[DOLLY_INVENTORY_ROWS][88];
    struct List header_rows;
    struct Node header_nodes[DOLLY_INVENTORY_HEADER_ROWS];
    char header_text[DOLLY_INVENTORY_HEADER_ROWS][DOLLY_INVENTORY_HEADER_BYTES];
    const char *clipboard_row;
    struct List details;
    struct Node detail_nodes[DOLLY_INVENTORY_DETAILS];
    char detail_text[DOLLY_INVENTORY_DETAILS][DOLLY_INVENTORY_DETAIL_BYTES];
    unsigned detail_count,selected_detail;
    DollyNativePartitionMap map;
    DollyMediaGeometry geometry;
    char driver[64];
    ULONG unit;
    char device_info[112];
    char chs_info[112];
    char geometry_info[160];
    char status[112];
    unsigned count;
    unsigned selected_partition;
    unsigned char verified;
} DollyNativeInventoryView;
int dolly_inventory_view_open(DollyNativeInventoryView *view,
                               struct Screen *screen,
                               const DollyEndpoint *source);
void dolly_inventory_view_pump(DollyNativeInventoryView *view);
void dolly_inventory_view_close(DollyNativeInventoryView *view);
#endif
