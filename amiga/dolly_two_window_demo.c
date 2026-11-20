#include "dolly_amiga_transfer_guard.h"
#include "dolly_amiga_fonts.h"
/* M1.12: responsive, two-window GadTools OperationSession demo.
 * MC68000 / AmigaOS 2.04: system pens, live window borders, aspect margins,
 * recessed 24x8 grid and incremental (not full-screen) progress painting.
 * Sheep Hide never cancels operation.
 */
#include "dolly_operation_session.h"
#include "dolly_gui_minimum.h"
#include "dolly_endpoint.h"
#include "dolly_amiga_endpoint_picker.h"
#include "dolly_amiga_device_query.h"
#include "dolly_amiga_destructive.h"
#include "dolly_amiga_inventory_view.h"
#include "dolly_amiga_surface_scan.h"
#include "dolly_amiga_log_view.h"
#include "dolly_amiga_image.h"
#include "dolly_amiga_io_settings.h"
#include "dolly_operation_pointer.h"
#include "dolly_amiga_transfer.h"
#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/gfxmacros.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
#include <dos/dosextens.h>
#include <libraries/gadtools.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <stdio.h>
#include <string.h>

/* libnix expects a pointer. NULL disables the Workbench console. */
const char *__stdiowin=NULL;

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;
struct Library *AslBase;
/* dos.library base is initialized by the C runtime; do not replace it. */

#define GRID_COLS 24u
#define GRID_ROWS 8u
#define GRID_TOTAL (GRID_COLS * GRID_ROWS)
#define COUNTER_UPDATE_STEPS 8u
static const char counter_heading[]="Surface Scan - Sheep Counter";
static const char image_heading[]="Create Image - Sheep Counter";
static const char grid_legend_prefix[]="1 grid = ";
static const char grid_legend_suffix[]=" sectors";
static const char show_log_label[]="Show Log";
static const char hide_label[]="Hide";
#define MAIN_DETAILS_ID 201u
#define SHEEP_HIDE_ID 203u
#define SHEEP_COUNT_ID 205u
#define SHEEP_ERRORS_ID 206u
#define SHEEP_LOG_ID 220u
#define SHEEP_PAUSE_ID 221u
#define MAIN_START_ID 208u
#define MAIN_OPERATION_ID 210u
#define MAIN_SOURCE_MODE_ID 220u
#define MAIN_TARGET_MODE_ID 221u
#define MAIN_IMAGE_FORMAT_ID 222u
#define MAIN_COMPRESSION_ID 223u
#define MAIN_COPY_MODE_ID 224u
#define MAIN_VET_MODE_ID 225u
#define MAIN_VET_REPAIR_ID 226u
#define MAIN_STATUS_ID 211u
#define MAIN_SOURCE_ID 212u
#define MAIN_TARGET_ID 213u
#define MAIN_DESC_ID 214u
#define MAIN_SETTINGS_ID 217u
#define MAIN_STOP_ID 218u
#define DOLLY_DEMO_BUILD_LABEL "Dolly Alpha 3.4"
#define MAIN_GADGET_HEIGHT 15
#define MAIN_COLUMN_GAP 6

typedef struct DemoLayout {
    LONG left, right, top, bottom;
    LONG endpoint_button_y,endpoint_frame_y,main_operation_y;
    LONG main_desc_y, main_status_y, main_progress_y;
    LONG progress_x, progress_percent_w, details_x, details_w;
    LONG selector_x[3],selector_w[3],selector_y,selector_h;
    LONG pad_x, pad_y;
    LONG controls_y, bar_y, grid_top, grid_bottom;
    LONG grid_x, grid_y, cell_w, cell_h, gap_x, gap_y;
    LONG grid_w, grid_h, grid_cols;
    LONG number_w, error_x, log_x, log_w, hide_x, hide_w, pause_x;
} DemoLayout;
typedef struct DollyDemoView {
    struct Window *window;
    struct Gadget *gadgets;
    struct Gadget *details, *settings, *stop;
    struct Gadget *start, *hide, *log, *pause, *count, *errors;
    const char *action_label;
    struct Gadget *device,*target_button;
    struct Gadget *description,*selectors[3],*operation_options[2],*status;
    STRPTR mode_labels[2][5];
    DollyEndpointMode mode_ids[2][4];
    unsigned mode_count[2],mode_active[2];
    char source_label[72],target_label[72],desc_label[96];
    char status_label[64];
    char block_label[48];
    char percent_label[8];
    APTR visual;
    unsigned char gadgets_detached;
    unsigned counter_steps;
    ULONG displayed_percent,displayed_errors;
    LONG bar_filled;
    DemoLayout l;
} DollyDemoView;

static struct Screen *public_screen;
static struct Menu *settings_menu;
#define IO_SETTINGS_ITEM FULLMENUNUM(0,0,NOSUB)

static struct NewMenu settings_items[]={
    {NM_TITLE,(STRPTR)"Settings",NULL,0,0,NULL},
    {NM_ITEM,(STRPTR)"I/O override...",NULL,0,0,NULL},
    {NM_END,NULL,NULL,0,0,NULL}
};
static struct DrawInfo *screen_info;
static DollyOperationSession operation;
static DollySurfaceScan surface;
static DollyAmigaTransfer transfer;
static int transfer_result;
static UWORD transfer_operation;
static char transfer_from[72],transfer_to[72];
static unsigned char scan_result,image_result;
static DollyAmigaImage image;
static char cell_result[GRID_TOTAL];
static DollyDemoView main_view, sheep_view;
static DollyEndpoint selected_source,selected_target;
static char source_file_status[64];
static DollyEndpointPicker source_picker,target_picker;
static DollyNativeInventoryView inventory_view;
static STRPTR demo_operations[]={
    (STRPTR)"Surface Scan",
    (STRPTR)"Inventory",
    (STRPTR)"Copy",
    (STRPTR)"Restore Image",
    (STRPTR)"Create Image",
    (STRPTR)"Disk Vet",NULL
};
static const char *demo_descriptions[]={
    "Read-only scan of source blocks.",
    "Inspect device and partition layout.",
    "Copy blocks from source to destination.",
    "Restore a file image to a destination.",
    "Create an image file from a selected source.",
    "Disk Vet: filesystem first aid (planned)."
};
static STRPTR image_types[]={(STRPTR)"RAW image",(STRPTR)"IFF DOLY",NULL};
static STRPTR compression_types[]={
    (STRPTR)"Not compressed",(STRPTR)"Fast",(STRPTR)"Compact",NULL
};
static STRPTR copy_modes[]={
    (STRPTR)"Sector-to-sector",(STRPTR)"Smart Copy",(STRPTR)"Clone and resize",NULL
};
static STRPTR vet_modes[]={
    (STRPTR)"Analyze (read only)",(STRPTR)"Analyze & repair",NULL
};
static STRPTR vet_repairs[]={
    (STRPTR)"Repair in place",(STRPTR)"File recovery",NULL
};
static unsigned image_type,compression_type,copy_mode,vet_mode,vet_repair;
static char recovery_drawer[256];
static DollyImageFormat selected_image_format(void) {
    static const DollyImageFormat formats[]={
        DOLLY_IMAGE_IFF_DOLY,DOLLY_IMAGE_IFF_FAST,DOLLY_IMAGE_IFF_COMPACT
    };
    return image_type?formats[compression_type]:DOLLY_IMAGE_RAW;
}
static UWORD selected_operation=0;
static unsigned berserk_mode=0; /* Future explicit opt-in; NORMAL only for now. */
static unsigned source_mode_mask(void) {
    return dolly_allowed_modes((DollyDemoOperation)selected_operation,
                               0u,DOLLY_ENDPOINT_NONE,berserk_mode);
}
static unsigned destination_mode_mask(void) {
    if(selected_source.kind==DOLLY_ENDPOINT_NONE ||
       !dolly_mode_allowed(source_mode_mask(),
                          dolly_endpoint_mode(&selected_source)))
        return 0u;
    return dolly_allowed_modes((DollyDemoOperation)selected_operation,
                               1u,selected_source.kind,berserk_mode);
}

static int recovery_destination(void) {
    return selected_operation==DOLLY_DEMO_DISK_VET && vet_mode==1u && vet_repair==1u;
}
static int destination_button_available(void) {
    if(recovery_destination() || selected_operation==DOLLY_DEMO_CREATE_IMAGE)
        return AslBase && (recovery_destination() || destination_mode_mask());
    return destination_mode_mask()!=0u;
}

static DollyEndpointMode medium_modes[2]={DOLLY_MODE_PHYSICAL,DOLLY_MODE_PHYSICAL};
static int main_cycle_rebuild;
static STRPTR blank_cycle_labels[]={(STRPTR)"",NULL};
static const char *const selector_titles[]={"Operation","Source","Destination"};
static STRPTR medium_names[]={
    (STRPTR)"Physical device",(STRPTR)"Logical device",
    (STRPTR)"Partition",(STRPTR)"File"
};

static void prepare_medium_modes(DollyDemoView *v) {
    const unsigned masks[]={source_mode_mask(),destination_mode_mask()};
    unsigned role,mode;
    for(role=0;role<2u;++role) {
        v->mode_count[role]=0;
        v->mode_active[role]=0;
        if(!dolly_mode_allowed(masks[role],medium_modes[role])) {
            for(mode=0;mode<4u;++mode)
                if(masks[role] & (1u<<mode)) {
                    medium_modes[role]=(DollyEndpointMode)mode;
                    break;
                }
        }
        for(mode=0;mode<4u;++mode) {
            unsigned index;
            if(!(masks[role] & (1u<<mode)))continue;
            index=v->mode_count[role]++;
            v->mode_labels[role][index]=medium_names[mode];
            v->mode_ids[role][index]=(DollyEndpointMode)mode;
            if(medium_modes[role]==(DollyEndpointMode)mode)v->mode_active[role]=index;
        }
        v->mode_labels[role][v->mode_count[role]]=NULL;
    }
}
static LONG selector_height(const struct TextFont *font) {
    LONG height=font->tf_YSize+4;
    return height>MAIN_GADGET_HEIGHT?height:MAIN_GADGET_HEIGHT;
}
static LONG place_main_rows(DemoLayout *d,const struct TextFont *font) {
    LONG row=d->selector_h=selector_height(font);
    d->main_operation_y=d->top+3;
    d->selector_y=d->main_operation_y+font->tf_YSize+4;
    d->endpoint_button_y=d->selector_y+row+4;
    d->endpoint_frame_y=d->endpoint_button_y+row+4;
    d->main_desc_y=d->endpoint_frame_y+row+5;
    d->controls_y=d->main_desc_y+row+6;
    d->main_progress_y=d->controls_y+row+4;
    d->main_status_y=d->main_progress_y+row+6;
    return d->main_status_y+row+3-d->top;
}
static LONG main_selector_width(struct RastPort *rp) {
    LONG width=0;
    unsigned i;
    static const char *const extra_labels[]={
        "<not required>","Destination...",
        "Sector-to-sector","Clone and resize","Not compressed",
        "Analyze (read only)","Analyze & repair","Repair in place","File recovery"
    };
    for(i=0;i<sizeof(extra_labels)/sizeof(extra_labels[0]);++i) {
        LONG label=TextLength(rp,(STRPTR)extra_labels[i],strlen(extra_labels[i]));
        if(label>width)width=label;
    }
    for(i=0;i<4u;++i) {
        LONG label=TextLength(rp,medium_names[i],strlen((const char *)medium_names[i]));
        if(label>width)width=label;
    }
    for(i=0;demo_operations[i];++i) {
        LONG label=TextLength(rp,demo_operations[i],strlen((const char *)demo_operations[i]));
        if(label>width)width=label;
    }
    {
        LONG details=2*(TextLength(rp,(STRPTR)"Details",7)+12);
        width+=30;
        if(width<details)width=details;
    }
    return 3*width+2*MAIN_COLUMN_GAP;
}

static void text_copy(char *dst,size_t cap,const char *src) {
    size_t n=src?strlen(src):0u;
    if(n>=cap)n=cap-1u;
    if(n)memcpy(dst,src,n);
    dst[n]='\0';
}
static const char *endpoint_basename(const char *path) {
    const char *name=path;
    for(;*path;++path)
        if(*path==':' || *path=='/')name=path+1;
    return *name?name:path;
}
static void format_endpoint(char *out,size_t cap,const DollyEndpoint *e) {
    char driver[DOLLY_PHYS_DRIVER_LEN];
    ULONG unit;
    if(e->kind==DOLLY_ENDPOINT_NONE) {
        text_copy(out,cap,"<not selected>");
    } else if(e->kind==DOLLY_ENDPOINT_FILE) {
        text_copy(out,cap,endpoint_basename(e->path));
    } else if((e->kind==DOLLY_ENDPOINT_UNIT || e->kind==DOLLY_ENDPOINT_PARTITION) &&
               dolly_amiga_parse_unit(e->path,driver,sizeof(driver),&unit)) {
        driver[strlen(driver)-7u]='\0';
        if(e->kind==DOLLY_ENDPOINT_PARTITION)
            snprintf(out,cap,"%s:%lu:%u",driver,(unsigned long)unit,
                     (unsigned)e->partition+1u);
        else snprintf(out,cap,"%s:%lu",driver,(unsigned long)unit);
    } else text_copy(out,cap,e->path[0]?e->path:"<unresolved>");
}
/* Text is pre-clipped against the live system font. Some early
 * GadTools versions do not implement later clipping attributes.
 */
static void clip_main(char *value,LONG width) {
    ULONG len;
    struct RastPort *rp;
    if(!main_view.window || width<1)return;
    rp=main_view.window->RPort;
    len=(ULONG)strlen(value);
    while(len && (LONG)TextLength(rp,(STRPTR)value,len)>width)
        value[--len]='\0';
}
static int can_start_operation(void) {
    if(selected_operation==DOLLY_DEMO_COPY && copy_mode!=0)
        return selected_source.kind==DOLLY_ENDPOINT_PARTITION &&
               selected_target.kind==DOLLY_ENDPOINT_PARTITION &&
               dolly_demo_can_start(DOLLY_DEMO_COPY,&selected_source,&selected_target);
    return dolly_demo_can_start((DollyDemoOperation)selected_operation,
                                &selected_source,&selected_target);
}
static int operation_active(void) {
    return operation.state==DOLLY_OP_RUNNING || operation.state==DOLLY_OP_PAUSED;
}
static int main_controls_busy(void) {
    return operation_active() || source_picker.window || target_picker.window;
}

static const char *action_label(int main_window) {
    if(operation.state==DOLLY_OP_PAUSED)return "Resume";
    if(operation.state==DOLLY_OP_RUNNING || !main_window)return "Pause";
    return "Start";
}
static int quit_blocked;
static const char *image_source_name(void) {
    const char *source=image.reader.source_path;
    const char *unit=strstr(source," / Unit ");
    const char *partition=unit?strstr(unit+8," / "):NULL;
    return partition?partition+3:source;
}
static int validating_restore(void) {
    return transfer.opened && transfer.job.source_checked<transfer.job.source_check_bytes;
}

static unsigned transfer_check_percent(void) {
    if(validating_restore()) {
        uint32_t sector=transfer.target.sector_bytes;
        return (unsigned)((transfer.job.source_checked/sector)*100u/
                          (transfer.job.source_check_bytes/sector));
    }
    return 100u;
}

static void update_main_status(void) {
    int need=dolly_demo_needs_target((DollyDemoOperation)selected_operation);
    int can=dolly_demo_can_start((DollyDemoOperation)selected_operation,
                                &selected_source,&selected_target);
    text_copy(main_view.desc_label,sizeof(main_view.desc_label),
              selected_operation==DOLLY_DEMO_COPY && copy_mode?
              (copy_mode==1?"Preview FFS allocated-block copy (read only).":"Analyze FFS partition sizes (read only)."):
              demo_descriptions[selected_operation]);
    if(quit_blocked && operation_active())
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Operation in progress. Cannot quit.");
    else if(operation.state==DOLLY_OP_PAUSED)
        text_copy(main_view.status_label,sizeof(main_view.status_label),"Operation paused.");
    else if(transfer_result && selected_operation==transfer_operation) {
        if(transfer.failure)
            text_copy(main_view.status_label,sizeof(main_view.status_label),transfer.failure);
        else if(operation.state==DOLLY_OP_COMPLETED)
            snprintf(main_view.status_label,sizeof(main_view.status_label),
                     "Copy complete: %.20s -> %.23s",transfer_from,transfer_to);
        else if(operation.state==DOLLY_OP_CANCELLED)
            text_copy(main_view.status_label,sizeof(main_view.status_label),
                      transfer.may_have_written?"Stopped. Destination may be partly overwritten.":
                                                "Stopped. No sectors were written.");
        else if(operation.state==DOLLY_OP_RUNNING && transfer_check_percent()<100u)
            snprintf(main_view.status_label,sizeof(main_view.status_label),
                     "Validating IFF DOLY: %u%%",transfer_check_percent());
        else if(operation.state==DOLLY_OP_RUNNING)
            snprintf(main_view.status_label,sizeof(main_view.status_label),
                     "%s: %.18s -> %.21s",
                     transfer.restoring?"Writing image":"Copying sectors",transfer_from,transfer_to);
    }
    else if(selected_operation==DOLLY_DEMO_CREATE_IMAGE && image_result && image.failure)
        text_copy(main_view.status_label,sizeof(main_view.status_label),image.failure);
    else if(selected_operation==DOLLY_DEMO_CREATE_IMAGE && image_result &&
            operation.state==DOLLY_OP_COMPLETED)
        snprintf(main_view.status_label,sizeof(main_view.status_label),
                 "Copy complete: %.20s -> %.23s",image_source_name(),image.name);
    else if(selected_operation==DOLLY_DEMO_CREATE_IMAGE && image_result &&
            operation.state==DOLLY_OP_CANCELLED)
        text_copy(main_view.status_label,sizeof(main_view.status_label),"Image cancelled.");
    else if(scan_result && selected_operation==DOLLY_DEMO_SCAN && surface.failure)
        text_copy(main_view.status_label,sizeof(main_view.status_label),surface.failure);
    else if(scan_result && selected_operation==DOLLY_DEMO_SCAN &&
            operation.state==DOLLY_OP_COMPLETED)
        sprintf(main_view.status_label,"Scan complete: %lu unreadable sectors.",
                (unsigned long)operation.errors);
    else if(scan_result && selected_operation==DOLLY_DEMO_SCAN &&
            operation.state==DOLLY_OP_CANCELLED)
        sprintf(main_view.status_label,"Scan stopped: %lu unreadable sectors.",
                (unsigned long)operation.errors);
    else if(operation.state==DOLLY_OP_RUNNING) {
        const char *kind=selected_source.kind==DOLLY_ENDPOINT_PARTITION?"partition":
                         selected_source.kind==DOLLY_ENDPOINT_LOGICAL?"volume":"disk";
        if(image_result)
            snprintf(main_view.status_label,sizeof(main_view.status_label),
                     "Writing %s to image: %.32s",kind,image.name);
        else if(scan_result)
            snprintf(main_view.status_label,sizeof(main_view.status_label),
                     "Scanning %s: %.38s",kind,surface.source_path);
        else
            text_copy(main_view.status_label,sizeof(main_view.status_label),
                      "Operation in progress.");
    }
    else if(selected_source.kind==DOLLY_ENDPOINT_FILE && source_file_status[0] &&
            dolly_mode_allowed(source_mode_mask(),DOLLY_MODE_FILE))
        text_copy(main_view.status_label,sizeof(main_view.status_label),source_file_status);
    else if(selected_operation==DOLLY_DEMO_INVENTORY && can)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Ready for read-only Inventory.");
    else if(selected_operation==DOLLY_DEMO_DISK_VET)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  recovery_destination()?(recovery_drawer[0]?
                    "Recovery drawer selected. File recovery is planned.":
                    "Choose a destination drawer for file recovery."):
                    "Disk Vet planned; no repairs available yet.");
    else if(!selected_source.kind)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Choose source.");
    else if(!dolly_mode_allowed(source_mode_mask(),
                                dolly_endpoint_mode(&selected_source)))
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Source incompatible. Choose a new source.");
    else if(need && !selected_target.kind)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Choose destination.");
    else if(need && !dolly_mode_allowed(destination_mode_mask(),
                                    dolly_endpoint_mode(&selected_target)))
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Destination incompatible. Choose again.");
    else if(selected_operation==DOLLY_DEMO_COPY && copy_mode!=0)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  can_start_operation()?"Ready for read-only FFS analysis.":"Choose two RDB partitions.");
    else if(!can)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Check source/destination identity or overlap.");
    else
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  selected_operation==DOLLY_DEMO_SCAN?
                  "Ready for read-only Surface Scan.":
                  selected_operation==DOLLY_DEMO_CREATE_IMAGE?
                  "Ready to create image.":"Ready. No disk I/O.");
    format_endpoint(main_view.source_label,sizeof(main_view.source_label),
                    &selected_source);
    if(recovery_destination())
        text_copy(main_view.target_label,sizeof(main_view.target_label),
                  recovery_drawer[0]?recovery_drawer:"<not selected>");
    else if(need)
        format_endpoint(main_view.target_label,sizeof(main_view.target_label),&selected_target);
    else text_copy(main_view.target_label,sizeof(main_view.target_label),"<not required>");
    if(main_view.window) {
        DemoLayout *d=&main_view.l;
        clip_main(main_view.source_label,d->selector_w[1]-8);
        clip_main(main_view.target_label,d->selector_w[2]-8);
        clip_main(main_view.desc_label,d->right-d->left-5);
        clip_main(main_view.status_label,d->right-d->left-8);
    }
}

/* Bounds-checked C99 decimal formatter. No format varargs or overflow. */
static void append_u32(char **out,char *limit,unsigned long n) {
    char digits[10];
    unsigned p=0;
    do {
        digits[p++]=(char)('0'+n%10ul);
        n/=10ul;
    } while(n && p<sizeof(digits));
    while(p && *out<limit)
        *(*out)++=digits[--p];
}
static int log_available(void) {
    return scan_result && surface.source_path[0] && !surface.source;
}
static const char *log_outcome(void) {
    if(surface.failure)return surface.failure;
    return surface.processed==surface.total?"Completed":"Stopped";
}
static int counter_available(void) {
    return operation.state==DOLLY_OP_RUNNING || operation.state==DOLLY_OP_PAUSED ||
           (operation.total_blocks &&
            ((scan_result && selected_operation==DOLLY_DEMO_SCAN) ||
             (image_result && selected_operation==DOLLY_DEMO_CREATE_IMAGE)));
}
static void progress_counts(uint64_t *current,uint64_t *total) {
    *current=operation.completed_blocks;
    *total=operation.total_blocks;
    if(validating_restore()) {
        uint32_t sector=transfer.target.sector_bytes;
        *current=transfer.job.source_checked/sector;
        *total=transfer.job.source_check_bytes/sector;
    }
}
static ULONG progress_units(ULONG units) {
    uint64_t current,total;
    progress_counts(&current,&total);
    return total?(ULONG)(current*units/total):0;
}
static void update_labels(DollyDemoView *v) {
    uint64_t current,total;
    char reached[21],count[21];
    progress_counts(&current,&total);
    dolly_surface_log_decimal(reached,current);
    dolly_surface_log_decimal(count,total);
    snprintf(v->block_label,sizeof(v->block_label),"%s / %s",reached,count);
}
static void fill(struct RastPort *rp,UWORD pen,
                 LONG x1,LONG y1,LONG x2,LONG y2) {
    if(x2<x1 || y2<y1)return;
    SetAPen(rp,pen);
    RectFill(rp,x1,y1,x2,y2);
}
static UWORD system_pen(unsigned id) {
    return screen_info->dri_Pens[id];
}
static void label(struct Window *w,LONG x,LONG y,const char *s) {
    struct RastPort *rp=w->RPort;
    SetDrMd(rp,JAM1);
    SetAPen(rp,system_pen(TEXTPEN));
    Move(rp,x,y);
    Text(rp,(STRPTR)s,(ULONG)strlen(s));
}
/* DrawInfo resolution expresses the physical size of a pixel on each axis. */
static void grid_pixel_size(LONG *pixel_x,LONG *pixel_y) {
    *pixel_x=screen_info->dri_Resolution.X;
    *pixel_y=screen_info->dri_Resolution.Y;
    if(*pixel_x<1 || *pixel_y<1)*pixel_x=*pixel_y=1;
}
static LONG grid_gap(LONG pixel,LONG other_pixel) {
    LONG larger=pixel>other_pixel?pixel:other_pixel;
    return (larger+pixel/2)/pixel;
}
static LONG sheep_button_width(struct RastPort *rp,const char *label) {
    LONG width=(LONG)TextLength(rp,(STRPTR)label,(ULONG)strlen(label))+12;
    return width>76?width:76;
}
static LONG digit_width(struct RastPort *rp) {
    LONG widest=0;
    char digit;
    for(digit='0';digit<='9';++digit) {
        LONG width=TextLength(rp,(STRPTR)&digit,1);
        if(width>widest)widest=width;
    }
    return widest;
}
static LONG sheep_action_width(struct RastPort *rp) {
    LONG log=sheep_button_width(rp,show_log_label);
    LONG resume=sheep_button_width(rp,"Resume");
    return log>resume?log:resume;
}
static LONG sheep_number_width(struct RastPort *rp) {
    LONG width=6*digit_width(rp)+TextLength(rp,(STRPTR)" / ",3)+12;
    return width>80?width:80;
}
static int fit_grid(DemoLayout *d,LONG width,LONG height) {
    LONG pixel_x,pixel_y,cols,best_span=0,best_unused=GRID_TOTAL;
    grid_pixel_size(&pixel_x,&pixel_y);
    d->gap_x=grid_gap(pixel_x,pixel_y);
    d->gap_y=grid_gap(pixel_y,pixel_x);
    /* Reflow the same cells without changing which sectors they represent. */
    for(cols=8;cols<=48;++cols) {
        LONG rows=((LONG)GRID_TOTAL+cols-1)/cols;
        LONG unused=cols*rows-GRID_TOTAL;
        LONG max_w=(width-(cols-1)*d->gap_x)/cols;
        LONG max_h=(height-(rows-1)*d->gap_y)/rows;
        LONG span=max_w*pixel_x<max_h*pixel_y?max_w*pixel_x:max_h*pixel_y;
        LONG cell_w=(span+pixel_x/2)/pixel_x;
        LONG cell_h=(span+pixel_y/2)/pixel_y;
        if(cell_w<8 || cell_h<8 || span<best_span ||
           (span==best_span && unused>=best_unused) ||
           cols*cell_w+(cols-1)*d->gap_x>width ||
           rows*cell_h+(rows-1)*d->gap_y>height)continue;
        best_span=span;
        best_unused=unused;
        d->grid_cols=cols;
        d->cell_w=cell_w;
        d->cell_h=cell_h;
        d->grid_w=cols*cell_w+(cols-1)*d->gap_x;
        d->grid_h=rows*cell_h+(rows-1)*d->gap_y;
    }
    return d->grid_cols!=0;
}
static int calculate_layout(DollyDemoView *v,int main_window) {
    struct Window *w=v->window;
    DemoLayout d;
    LONG avail_w,avail_h;
    if(!w)return 0;
    memset(&d,0,sizeof(d));
    /* Intuition supplies actual chrome thickness; no guessed borders.
     * DrawInfo supplies the display pixel aspect ratio (V36+).
     */
    d.pad_x=(screen_info->dri_Resolution.X==screen_info->dri_Resolution.Y)?2:4;
    d.pad_y=2;
    d.left=(LONG)w->BorderLeft+d.pad_x;
    d.right=(LONG)w->Width-(LONG)w->BorderRight-d.pad_x-1;
    d.top=(LONG)w->BorderTop+d.pad_y;
    d.bottom=(LONG)w->Height-(LONG)w->BorderBottom-d.pad_y-1;
    avail_w=d.right-d.left+1;
    avail_h=d.bottom-d.top+1;
    d.selector_h=selector_height(w->RPort->Font);
    d.progress_percent_w=3*digit_width(w->RPort)+TextLength(w->RPort,(STRPTR)"%",1);
    if(main_window) {
        LONG column_width=(avail_w-2*MAIN_COLUMN_GAP)/3;
        unsigned column;
        if(avail_w<main_selector_width(w->RPort) ||
           avail_h<place_main_rows(&d,w->RPort->Font))return 0;
        for(column=0;column<3u;++column) {
            d.selector_x[column]=d.left+(LONG)column*(column_width+MAIN_COLUMN_GAP);
            d.selector_w[column]=column_width;
        }
        d.details_w=column_width/2;
        d.details_x=d.selector_x[2]+column_width-d.details_w;
        d.progress_x=d.left+TextLength(w->RPort,(STRPTR)"Progress:",9)+8;
    } else {
        if(avail_w<248 || avail_h<140)return 0;
        d.hide_w=sheep_button_width(w->RPort,hide_label);
        d.hide_x=d.right-d.hide_w+1;
        d.log_w=sheep_action_width(w->RPort);
        d.log_x=d.hide_x-d.log_w-6;
        d.pause_x=d.log_x-d.log_w-6;
        d.controls_y=d.bottom-d.selector_h+1;
        d.bar_y=d.controls_y-w->RPort->TxHeight-8-d.selector_h;
        d.grid_top=d.top+w->RPort->TxHeight+5;
        d.grid_bottom=d.bar_y-5;
        if(!fit_grid(&d,avail_w-4,d.grid_bottom-d.grid_top-3))return 0;
        d.grid_x=d.left+(avail_w-d.grid_w)/2;
        d.grid_y=d.grid_top+(d.grid_bottom-d.grid_top+1-d.grid_h)/2;
        d.number_w=(d.pause_x-d.left-12)/2;
        d.error_x=d.left+d.number_w+6;
        if(d.number_w<sheep_number_width(w->RPort))return 0;
    }
    v->l=d;
    return 1;
}
static struct Gadget *make_button(struct Gadget *last,DollyDemoView *v,
                                  UWORD id,LONG x,LONG y,LONG width,
                                  const char *title) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=(WORD)x;
    ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)width;
    ng.ng_Height=(WORD)v->l.selector_h;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)title;
    return CreateGadget(BUTTON_KIND,last,&ng,
                        GA_Disabled,(ULONG)(
                          (id==SHEEP_LOG_ID && !log_available()) ||
                          (id==MAIN_STOP_ID &&
                           !operation_active()) ||
                          (id==MAIN_DETAILS_ID && !operation_active()) ||
                          (id==MAIN_START_ID && !operation_active() && !can_start_operation()) ||
                          (id==SHEEP_PAUSE_ID && !operation_active()) ||
                          (id==MAIN_TARGET_ID && !destination_button_available())),
                        TAG_DONE);
}
static struct Gadget *make_number(struct Gadget *last,DollyDemoView *v,
                                  UWORD id,LONG x,LONG y,LONG width,
                                  ULONG value) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=(WORD)x;
    ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)width;
    ng.ng_Height=(WORD)v->l.selector_h;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(NUMBER_KIND,last,&ng,
                        GTNM_Number,value,GTNM_Border,TRUE,TAG_DONE);
}
static struct Gadget *make_text(struct Gadget *last,DollyDemoView *v,
                                UWORD id,LONG x,LONG y,LONG width,
                                char *value,ULONG border) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=(WORD)x;
    ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)width;
    ng.ng_Height=(WORD)v->l.selector_h;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)value,
                        GTTX_Border,border,
                        GTTX_Clipped,TRUE,TAG_DONE);
}
static struct Gadget *make_selector(struct Gadget *last,DollyDemoView *v,
                                    UWORD id,unsigned column,LONG y,STRPTR *choices,
                                    unsigned active,int unavailable) {
    struct NewGadget ng={0};
    int disabled=unavailable || main_controls_busy();
    int blank=unavailable || (disabled && !operation_active());
    LONG x=v->l.selector_x[column];
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=dolly_amiga_text_attr(0);
    ng.ng_LeftEdge=(WORD)x;
    ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)v->l.selector_w[column];
    ng.ng_Height=(WORD)v->l.selector_h;
    ng.ng_GadgetID=id;
    if(disabled && GadToolsBase->lib_Version<37u) {
        ng.ng_GadgetText=blank?blank_cycle_labels[0]:choices[active];
        return CreateGadget(BUTTON_KIND,last,&ng,GA_Disabled,TRUE,TAG_DONE);
    }
    return CreateGadget(CYCLE_KIND,last,&ng,
                        GTCY_Labels,(ULONG)(blank?blank_cycle_labels:choices),
                        GTCY_Active,(ULONG)(blank?0u:active),
                        GA_Disabled,(ULONG)disabled,
                        TAG_DONE);
}
static int rebuild_gadgets(DollyDemoView *v,int main_window) {
    struct Gadget *last;
    DemoLayout *d=&v->l;
    if(!calculate_layout(v,main_window))return 0;
    update_labels(v);
    if(main_window)main_cycle_rebuild=0;
    if(v->gadgets) {
        /* IDCMP_SIZEVERIFY already detached the list before sizing.
         * Never remove the same gadgets twice.
         */
        if(!v->gadgets_detached)
            RemoveGList(v->window,v->gadgets,-1);
        FreeGadgets(v->gadgets);
        v->gadgets=NULL;
        v->details=NULL;v->settings=NULL;
        v->stop=NULL;v->start=NULL;v->hide=NULL;
        v->count=NULL;v->errors=NULL;v->log=NULL;v->pause=NULL;
        v->device=NULL;v->target_button=NULL;
        v->description=NULL;v->status=NULL;
        memset(v->selectors,0,sizeof(v->selectors));
        memset(v->operation_options,0,sizeof(v->operation_options));
    }
    last=CreateContext(&v->gadgets);
    if(!last)return 0;
    if(main_window) {
        unsigned role;
        prepare_medium_modes(v);
        update_main_status();
        last=make_button(last,v,MAIN_SOURCE_ID,d->selector_x[1],d->endpoint_button_y,
                         d->selector_w[1],"Source...");
        if(!last)return 0;
        v->device=last;
        last=make_button(last,v,MAIN_TARGET_ID,d->selector_x[2],d->endpoint_button_y,
                         d->selector_w[2],"Destination...");
        if(!last)return 0;
        v->target_button=last;
        last=make_selector(last,v,MAIN_OPERATION_ID,0,d->selector_y,
                           demo_operations,selected_operation,0);
        if(!last)return 0;
        v->selectors[0]=last;
        for(role=0;role<2u;++role) {
            last=make_selector(last,v,role?MAIN_TARGET_MODE_ID:MAIN_SOURCE_MODE_ID,
                               role+1u,d->selector_y,v->mode_labels[role],
                               v->mode_active[role],!v->mode_count[role]);
            if(!last)return 0;
            v->selectors[role+1u]=last;
        }
        if(selected_operation==DOLLY_DEMO_CREATE_IMAGE) {
            last=make_selector(last,v,MAIN_IMAGE_FORMAT_ID,0,d->endpoint_button_y,
                               image_types,image_type,0);
            if(!last)return 0;
            v->operation_options[0]=last;
            last=make_selector(last,v,MAIN_COMPRESSION_ID,0,d->endpoint_frame_y,
                               compression_types,compression_type,!image_type);
            if(!last)return 0;
            v->operation_options[1]=last;
        } else if(selected_operation==DOLLY_DEMO_COPY) {
            last=make_selector(last,v,MAIN_COPY_MODE_ID,0,d->endpoint_button_y,
                               copy_modes,copy_mode,0);
            if(!last)return 0;
            v->operation_options[0]=last;
        } else if(selected_operation==DOLLY_DEMO_DISK_VET) {
            last=make_selector(last,v,MAIN_VET_MODE_ID,0,d->endpoint_button_y,
                               vet_modes,vet_mode,0);
            if(!last)return 0;
            v->operation_options[0]=last;
            last=make_selector(last,v,MAIN_VET_REPAIR_ID,0,d->endpoint_frame_y,
                               vet_repairs,vet_repair,!vet_mode);
            if(!last)return 0;
            v->operation_options[1]=last;
        }
        last=make_text(last,v,MAIN_DESC_ID,d->left+3,d->main_desc_y,
                       d->right-d->left-5,v->desc_label,FALSE);
        if(!last)return 0;
        v->description=last;
        last=make_button(last,v,MAIN_SETTINGS_ID,d->selector_x[0],d->controls_y,
                         d->selector_w[0],"Settings");
        if(!last)return 0;
        v->settings=last;
        last=make_button(last,v,MAIN_START_ID,d->selector_x[1],d->controls_y,
                         d->selector_w[1],"");
        if(!last)return 0;
        v->start=last;
        last=make_button(last,v,MAIN_STOP_ID,d->selector_x[2],d->controls_y,
                         d->selector_w[2],"Stop");
        if(!last)return 0;
        v->stop=last;
        last=make_button(last,v,MAIN_DETAILS_ID,d->details_x,d->main_progress_y,
                         d->details_w,"Details");
        if(!last)return 0;
        v->details=last;
        last=make_text(last,v,MAIN_STATUS_ID,d->left+3,d->main_status_y,
                       d->right-d->left-5,v->status_label,FALSE);
        if(!last)return 0;
        v->status=last;
    } else {
        last=make_text(last,v,SHEEP_COUNT_ID,d->left,d->controls_y,
                       d->number_w,v->block_label,TRUE);
        if(!last)return 0;
        v->count=last;
        last=make_number(last,v,SHEEP_ERRORS_ID,d->error_x,d->controls_y,
                         d->number_w,(ULONG)operation.errors);
        if(!last)return 0;
        v->errors=last;
        v->displayed_errors=(ULONG)operation.errors;
        last=make_button(last,v,SHEEP_PAUSE_ID,d->pause_x,d->controls_y,
                         d->log_w,action_label(0));
        if(!last)return 0;
        v->pause=last;
        last=make_button(last,v,SHEEP_LOG_ID,d->log_x,d->controls_y,
                         d->log_w,show_log_label);
        if(!last)return 0;
        v->log=last;
        last=make_button(last,v,SHEEP_HIDE_ID,d->hide_x,d->controls_y,
                         d->hide_w,hide_label);
        if(!last)return 0;
        v->hide=last;
    }
    AddGList(v->window,v->gadgets,(ULONG)-1,-1,NULL);
    v->gadgets_detached=0;
    RefreshGList(v->gadgets,v->window,NULL,-1);
    GT_RefreshWindow(v->window,NULL);
    v->action_label=action_label(main_window);
    return 1;
}
static void destroy_view(DollyDemoView *v) {
    if(v->window) {
        if(v->gadgets && v->gadgets_detached) {
            /* Already detached; leave the list freeable below. */
            v->gadgets_detached=0;
        }
        CloseWindow(v->window);
        v->window=NULL;
    }
    if(v->gadgets) {FreeGadgets(v->gadgets);v->gadgets=NULL;}
    if(v->visual) {FreeVisualInfo(v->visual);v->visual=NULL;}
    v->details=NULL;v->settings=NULL;
    v->stop=NULL;v->start=NULL;v->hide=NULL;
    v->count=NULL;v->errors=NULL;
    v->device=NULL;v->target_button=NULL;
    v->description=NULL;v->status=NULL;
        memset(v->selectors,0,sizeof(v->selectors));
}
/* Measure the actual active font and window chrome, then tell Intuition
 * the smallest outer rectangle that fits the required GadTools controls.
 * Optional explanatory prose is not a sizing constraint.
 */
static int set_content_minimum(DollyDemoView *v,int main_window) {
    DollyGuiMinMetrics m;
    DollyGuiMinimum minimum;
    struct Window *w=v->window;
    struct RastPort *rp=w->RPort;
    memset(&m,0,sizeof(m));
    if(!rp->Font)return 0;
    m.border_left=(ULONG)w->BorderLeft;
    m.border_right=(ULONG)w->BorderRight;
    m.border_top=(ULONG)w->BorderTop;
    m.border_bottom=(ULONG)w->BorderBottom;
    m.padding_x=(screen_info->dri_Resolution.X==
                 screen_info->dri_Resolution.Y)?2u:4u;
    m.padding_y=2u;
    m.line_height=(ULONG)rp->Font->tf_YSize;
    if(main_window) {
        DemoLayout d={0};
        LONG width=main_selector_width(rp)+w->BorderLeft+w->BorderRight+2*m.padding_x;
        LONG height=place_main_rows(&d,rp->Font)+w->BorderTop+w->BorderBottom+2*m.padding_y;
        if(width>w->Width || height>w->Height)return 0;
        return WindowLimits(w,width,w->Height,0,w->Height)!=FALSE;
    }
    {
        ULONG scan_width=(ULONG)TextLength(rp,(STRPTR)counter_heading,
                sizeof(counter_heading)-1);
        ULONG image_width=(ULONG)TextLength(rp,(STRPTR)image_heading,
                sizeof(image_heading)-1);
        m.heading_width=(scan_width>image_width?scan_width:image_width)+8u+10u*(ULONG)digit_width(rp)+
                (ULONG)TextLength(rp,(STRPTR)grid_legend_prefix,
                sizeof(grid_legend_prefix)-1)+
                (ULONG)TextLength(rp,(STRPTR)grid_legend_suffix,
                sizeof(grid_legend_suffix)-1);
        m.hide_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)hide_label,sizeof(hide_label)-1);
        m.log_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)show_log_label,sizeof(show_log_label)-1);
    }
    {
        ULONG footer_width=2*sheep_number_width(rp)+24+
                          2*sheep_action_width(rp)+sheep_button_width(rp,hide_label);
        if(m.heading_width<footer_width)m.heading_width=footer_width;
    }
    minimum=dolly_gui_measure_minimum(DOLLY_GUI_WINDOW_SHEEP,m);
    if(!minimum.valid)return 0;
    {
        LONG pixel_x,pixel_y,span;
        ULONG width,height;
        grid_pixel_size(&pixel_x,&pixel_y);
        span=8*(pixel_x>pixel_y?pixel_x:pixel_y);
        width=m.border_left+m.border_right+2*m.padding_x+6+
              GRID_COLS*((span+pixel_x/2)/pixel_x)+
              (GRID_COLS-1)*grid_gap(pixel_x,pixel_y);
        height=m.border_top+m.border_bottom+2*m.padding_y+2*m.line_height+2*(ULONG)selector_height(rp->Font)+21u+
               GRID_ROWS*((span+pixel_y/2)/pixel_y)+
               (GRID_ROWS-1)*grid_gap(pixel_y,pixel_x);
        if(minimum.outer_width<width)minimum.outer_width=width;
        if(minimum.outer_height<height)minimum.outer_height=height;
    }
    if(minimum.outer_width>(ULONG)w->Width ||
       minimum.outer_height>(ULONG)w->Height)return 0;
    return WindowLimits(w,(LONG)minimum.outer_width,
                        (LONG)minimum.outer_height,0,0)!=FALSE;
}
static int open_view(DollyDemoView *v,int main_window,const char *title,
                     ULONG left,ULONG top,ULONG width,ULONG height) {
    v->visual=GetVisualInfo(public_screen,TAG_DONE);
    if(!v->visual)return 0;
    v->window=OpenWindowTags(NULL,
        WA_PubScreen,(ULONG)public_screen,
        WA_Title,(ULONG)title,
        WA_Left,left,WA_Top,top,
        WA_Width,width,WA_Height,height,
        /* Safe initial limits; replaced by measured WindowLimits() below. */
        WA_MinWidth,100,WA_MinHeight,75,
        WA_MaxWidth,(ULONG)public_screen->Width,
        WA_MaxHeight,(ULONG)public_screen->Height-
                     ((ULONG)public_screen->BarHeight+1ul),
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,
        WA_CloseGadget,TRUE,WA_SizeGadget,TRUE,
        WA_Activate,TRUE,WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|
                 IDCMP_REFRESHWINDOW|IDCMP_NEWSIZE|IDCMP_SIZEVERIFY|
                 (main_window?IDCMP_MENUPICK:0),
        TAG_DONE);
    if(!v->window)return 0;
    dolly_amiga_window_font(v->window,0);
    if(!set_content_minimum(v,main_window))return 0;
    return rebuild_gadgets(v,main_window);
}
static void clear_client(DollyDemoView *v) {
    struct Window *w=v->window;
    fill(w->RPort,system_pen(BACKGROUNDPEN),
         w->BorderLeft,w->BorderTop,
         (LONG)w->Width-w->BorderRight-1,
         (LONG)w->Height-w->BorderBottom-1);
}
static void update_numbers(DollyDemoView *v) {
    if(!v->window || !v->count)return;
    /* Keep the formatted counter in view-owned storage. */
    update_labels(v);
    GT_SetGadgetAttrs(v->count,v->window,NULL,
                      GTTX_Text,(ULONG)v->block_label,TAG_DONE);
    v->counter_steps=0;
    if(v->errors && v->displayed_errors!=(ULONG)operation.errors) {
        GT_SetGadgetAttrs(v->errors,v->window,NULL,
                          GTNM_Number,(ULONG)operation.errors,TAG_DONE);
    }
    v->displayed_errors=(ULONG)operation.errors;
}
static void ghost_area(struct RastPort *rp,LONG x,LONG y,LONG right,LONG bottom) {
    static UWORD pattern[]={0xaaaa,0x5555};
    SetDrMd(rp,JAM1);
    SetAfPt(rp,pattern,1);
    fill(rp,system_pen(BACKGROUNDPEN),x,y,right,bottom);
    SetAfPt(rp,NULL,0);
}
static void draw_endpoint_frame(unsigned column,const char *caption,int inactive) {
    DemoLayout *d=&main_view.l;
    struct RastPort *rp=main_view.window->RPort;
    struct TagItem tags[]={
        {GT_VisualInfo,(ULONG)main_view.visual},
        {GTBB_Recessed,TRUE},
        {TAG_DONE,0}
    };
    LONG x=d->selector_x[column],y=d->endpoint_frame_y;
    LONG width=d->selector_w[column],height=d->selector_h;
    SetDrMd(rp,JAM1);
    fill(rp,system_pen(BACKGROUNDPEN),x+2,y+2,x+width-3,y+height-3);
    DrawBevelBoxA(rp,x,y,width,height,tags);
    SetAPen(rp,system_pen(TEXTPEN));
    Move(rp,x+4,y+(height-rp->TxHeight)/2+rp->TxBaseline);
    Text(rp,(STRPTR)caption,strlen(caption));
    /* This frame is painted directly, so GadTools cannot ghost it. */
    if(inactive)ghost_area(rp,x,y,x+width-1,y+height-1);
}

static void draw_endpoint_frames(void) {
    int busy=main_controls_busy();
    draw_endpoint_frame(1,main_view.source_label,busy || !source_mode_mask());
    draw_endpoint_frame(2,main_view.target_label,
                        busy || (!destination_mode_mask() && !destination_button_available()));
}

static void draw_main_start_label(void) {
    DemoLayout *d=&main_view.l;
    struct RastPort *rp=main_view.window->RPort;
    const char *caption=main_view.action_label;
    LONG x,y,width;
    if(!main_view.start || !caption)return;
    width=TextLength(rp,(STRPTR)caption,strlen(caption));
    SetDrMd(rp,JAM1);
    SetAPen(rp,system_pen(SHINEPEN));
    x=d->selector_x[1]+(d->selector_w[1]-width)/2;
    y=d->controls_y+(d->selector_h-rp->TxHeight)/2;
    Move(rp,x,y+rp->TxBaseline);
    Text(rp,(STRPTR)caption,strlen(caption));
    if(main_view.start->Flags&GFLG_DISABLED)
        ghost_area(rp,x,y,x+width-1,y+rp->TxHeight-1);
}
/* Both windows reserve space for the percentage inside the same recessed
 * well. Sheep Counter repaints only the changed fill and caption. */
static void draw_progress_well(DollyDemoView *v,LONG left,LONG y,LONG right,int full) {
    struct Window *w=v->window;
    struct RastPort *rp=w->RPort;
    LONG h=v->l.selector_h;
    LONG caption_x=right-v->l.progress_percent_w-5;
    LONG fill_width=caption_x-left-8;
    LONG filled=(LONG)progress_units((ULONG)fill_width);
    LONG previous=v->bar_filled;
    ULONG percent=progress_units(100);
    SetDrMd(rp,JAM1);
    if(full) {
        struct TagItem tags[]={
            {GT_VisualInfo,(ULONG)v->visual},
            {GTBB_Recessed,TRUE},
            {TAG_DONE,0}
        };
        DrawBevelBoxA(rp,left,y,right-left+1,h,tags);
        fill(rp,system_pen(BACKGROUNDPEN),left+2,y+2,right-2,y+h-3);
        previous=0;
    }
    if(filled>previous)
        fill(rp,system_pen(FILLPEN),left+2+previous,y+2,left+1+filled,y+h-3);
    else if(filled<previous)
        fill(rp,system_pen(BACKGROUNDPEN),left+2+filled,y+2,left+1+previous,y+h-3);
    if(full || percent!=v->displayed_percent) {
        char *end=v->percent_label;
        LONG width;
        append_u32(&end,v->percent_label+sizeof(v->percent_label)-2,percent);
        *end++='%';
        *end='\0';
        width=TextLength(rp,(STRPTR)v->percent_label,end-v->percent_label);
        fill(rp,system_pen(BACKGROUNDPEN),caption_x,y+2,right-2,y+h-3);
        label(w,right-width-5,y+(h-rp->TxHeight)/2+rp->TxBaseline,v->percent_label);
    }
    v->displayed_percent=percent;
    v->bar_filled=filled;
}
static void draw_main_progress(void) {
    DemoLayout *d=&main_view.l;
    struct Window *w=main_view.window;
    struct RastPort *rp=w->RPort;
    LONG right=d->details_x-MAIN_COLUMN_GAP-1;
    LONG y=d->main_progress_y,h=d->selector_h;
    if(main_view.gadgets_detached)return;
    SetDrMd(rp,JAM1);
    fill(rp,system_pen(BACKGROUNDPEN),d->left,y,right,y+h-1);
    label(w,d->left,y+(h-rp->TxHeight)/2+rp->TxBaseline,"Progress:");
    draw_progress_well(&main_view,d->progress_x,y,right,1);
    if(!operation_active())ghost_area(rp,d->left,y,right,y+h-1);
}


static void draw_main_static(void) {
    DemoLayout *d=&main_view.l;
    struct TagItem tags[3];
    if(!main_view.window)return;
    unsigned column;
    for(column=0;column<3u;++column)
        label(main_view.window,d->selector_x[column],
              d->main_operation_y+main_view.window->RPort->TxBaseline,
              selector_titles[column]);
    tags[0].ti_Tag=GT_VisualInfo;
    tags[0].ti_Data=(ULONG)main_view.visual;
    tags[1].ti_Tag=GTBB_Recessed;
    tags[1].ti_Data=TRUE;
    tags[2].ti_Tag=TAG_DONE;
    tags[2].ti_Data=0;
    DrawBevelBoxA(main_view.window->RPort,d->left,d->main_status_y-2,
                  d->right-d->left+1,d->selector_h+4,tags);
    DrawBevelBoxA(main_view.window->RPort,d->left,d->main_desc_y-2,
                  d->right-d->left+1,d->selector_h+4,tags);
    draw_endpoint_frames();
}
static void draw_marker(struct RastPort *rp,LONG x,LONG y,
                        LONG width,LONG height,char marker,ULONG pen) {
    const struct TextFont *font=rp->Font;
    UBYTE mode=rp->DrawMode;
    LONG glyph_width;
    glyph_width=(LONG)TextLength(rp,(STRPTR)&marker,1);
    if(glyph_width>width || font->tf_YSize>height)return;
    SetDrMd(rp,JAM1);
    SetAPen(rp,system_pen(pen));
    Move(rp,x+(width-glyph_width)/2,
         y+(height-(LONG)font->tf_YSize)/2+
           (LONG)font->tf_Baseline);
    Text(rp,(STRPTR)&marker,1);
    SetDrMd(rp,mode);
}
static void draw_cell(unsigned idx,unsigned completed,int running) {
    DemoLayout *d=&sheep_view.l;
    struct RastPort *rp=sheep_view.window->RPort;
    LONG x=d->grid_x+(LONG)(idx%(unsigned)d->grid_cols)*(d->cell_w+d->gap_x);
    LONG y=d->grid_y+(LONG)(idx/(unsigned)d->grid_cols)*(d->cell_h+d->gap_y);
    char marker=cell_result[idx];
    ULONG background=idx<completed?FILLPEN:SHINEPEN;
    ULONG foreground=TEXTPEN;
    if(marker=='X') {
        background=SHADOWPEN;
        foreground=SHINEPEN;
    } else if(marker=='R') {
        background=BACKGROUNDPEN;
    } else if(running && idx==completed) {
        marker='*';
    }
    fill(rp,system_pen(background),x,y,x+d->cell_w-1,y+d->cell_h-1);
    if(marker)draw_marker(rp,x,y,d->cell_w,d->cell_h,marker,foreground);
}
static void draw_sheep_static(void) {
    DemoLayout *d=&sheep_view.l;
    struct RastPort *rp;
    struct TagItem tags[3];
    unsigned i,completed=progress_units(GRID_TOTAL);
    const char *heading=transfer_result?
                       (validating_restore()?"Validating IFF DOLY...":
                        transfer.restoring?"Restoring image...":"Copying sectors..."):
                       image_result?image_heading:counter_heading;
    if(!sheep_view.window)return;
    rp=sheep_view.window->RPort;
    label(sheep_view.window,d->left,d->top+rp->TxBaseline,heading);
    if((scan_result || image_result || transfer_result) && operation.total_blocks) {
        char legend[sizeof(grid_legend_prefix)+10+sizeof(grid_legend_suffix)];
        char *p=legend+sizeof(grid_legend_prefix)-1;
        LONG width;
        /* Proportional cells may differ by one sector. Show the upper
         * bound, rounded up so that even a small scan shows one sector. */
        uint64_t shown,total;
        progress_counts(&shown,&total);
        memcpy(legend,grid_legend_prefix,sizeof(grid_legend_prefix)-1);
        append_u32(&p,p+10,(unsigned long)(total/GRID_TOTAL+
                   (total%GRID_TOTAL!=0)));
        memcpy(p,grid_legend_suffix,sizeof(grid_legend_suffix));
        width=(LONG)TextLength(rp,(STRPTR)legend,(ULONG)strlen(legend));
        label(sheep_view.window,d->right+1-width,d->top+rp->TxBaseline,legend);
    }
    tags[0].ti_Tag=GT_VisualInfo;
    tags[0].ti_Data=(ULONG)sheep_view.visual;
    tags[1].ti_Tag=GTBB_Recessed;
    tags[1].ti_Data=TRUE;
    tags[2].ti_Tag=TAG_DONE;
    tags[2].ti_Data=0;
    DrawBevelBoxA(rp,d->left,d->grid_top,
                  d->right-d->left+1,d->grid_bottom-d->grid_top+1,tags);
    for(i=0;i<GRID_TOTAL;++i)
        draw_cell(i,completed,operation.state==DOLLY_OP_RUNNING);
    draw_progress_well(&sheep_view,d->left,d->bar_y,d->right,1);
    label(sheep_view.window,d->left,d->controls_y-3,"Blocks:");
    label(sheep_view.window,d->error_x,d->controls_y-3,"Errors:");
}
static void draw_full(DollyDemoView *v,int main_window) {
    clear_client(v);
    if(main_window)draw_main_static();
    else draw_sheep_static();
    RefreshGList(v->gadgets,v->window,NULL,-1);
    GT_RefreshWindow(v->window,NULL);
    if(main_window) {
        draw_main_progress();
        draw_main_start_label();
    }
}
static int show_sheep(void) {
    ULONG top=(ULONG)public_screen->BarHeight+1ul;
    if(!counter_available())return 0;
    if(sheep_view.window)return 1;
    if(!open_view(&sheep_view,0,"Dolly - Sheep Counter",
                  0,top,(ULONG)public_screen->Width,
                  (ULONG)public_screen->Height-top)) {
        destroy_view(&sheep_view);
        return 0;
    }
    dolly_operation_show_sheep_counter(&operation);
    draw_full(&sheep_view,0);
    update_numbers(&sheep_view);
    return 1;
}
static void hide_sheep(void) {
    destroy_view(&sheep_view);
    dolly_operation_hide_sheep_counter(&operation);
}
static int io_settings_available(void) {
    return operation.state!=DOLLY_OP_RUNNING &&
           operation.state!=DOLLY_OP_PAUSED &&
           !source_picker.window && !target_picker.window;
}
static void set_cycle_state(struct Gadget *g,STRPTR *labels,unsigned active,int unavailable) {
    int disabled=unavailable || main_controls_busy();
    int blank=unavailable || (disabled && !operation_active());
    if(!g || ((g->Flags&GFLG_DISABLED)!=0)==disabled)return;
    if(GadToolsBase->lib_Version<37u) {
        main_cycle_rebuild=1;
        return;
    }
    GT_SetGadgetAttrs(g,main_view.window,NULL,
                      GTCY_Labels,(ULONG)(blank?blank_cycle_labels:labels),
                      GTCY_Active,(ULONG)(blank?0u:active),
                      GA_Disabled,(ULONG)disabled,TAG_DONE);
}
static void update_main_actions(void) {
    int active=operation_active();
    if(sheep_view.window && sheep_view.pause)
        GT_SetGadgetAttrs(sheep_view.pause,sheep_view.window,NULL,
                          GA_Disabled,(ULONG)!active,TAG_DONE);
    if(sheep_view.window && sheep_view.log)
        GT_SetGadgetAttrs(sheep_view.log,sheep_view.window,NULL,
                          GA_Disabled,(ULONG)!log_available(),TAG_DONE);
    if(!main_view.window)return;
    if(settings_menu) {
        if(io_settings_available())OnMenu(main_view.window,IO_SETTINGS_ITEM);
        else OffMenu(main_view.window,IO_SETTINGS_ITEM);
    }
    if(main_view.settings)
        GT_SetGadgetAttrs(main_view.settings,main_view.window,NULL,
                          GA_Disabled,(ULONG)!io_settings_available(),TAG_DONE);
    if(main_view.stop)
        GT_SetGadgetAttrs(main_view.stop,main_view.window,NULL,
                          GA_Disabled,(ULONG)!active,TAG_DONE);
    if(main_view.details)
        GT_SetGadgetAttrs(main_view.details,main_view.window,NULL,
                          GA_Disabled,(ULONG)!active,TAG_DONE);
    if(main_view.start)
        GT_SetGadgetAttrs(main_view.start,main_view.window,NULL,
                          GA_Disabled,(ULONG)(!active && (source_picker.window || target_picker.window ||
                          !can_start_operation())),TAG_DONE);
    if(main_view.device)
        GT_SetGadgetAttrs(main_view.device,main_view.window,NULL,
                          GA_Disabled,(ULONG)(active || !source_mode_mask() ||
                          source_picker.window || target_picker.window),TAG_DONE);
    if(main_view.target_button)
        GT_SetGadgetAttrs(main_view.target_button,main_view.window,NULL,
                          GA_Disabled,(ULONG)(active || !destination_button_available() ||
                          source_picker.window || target_picker.window),TAG_DONE);
    {
        unsigned role;
        set_cycle_state(main_view.selectors[0],demo_operations,selected_operation,0);
        for(role=0;role<2u;++role)
            set_cycle_state(main_view.selectors[role+1u],main_view.mode_labels[role],
                            main_view.mode_active[role],!main_view.mode_count[role]);
        if(selected_operation==DOLLY_DEMO_CREATE_IMAGE) {
            set_cycle_state(main_view.operation_options[0],image_types,image_type,0);
            set_cycle_state(main_view.operation_options[1],compression_types,compression_type,
                            !image_type);
        } else if(selected_operation==DOLLY_DEMO_DISK_VET) {
            set_cycle_state(main_view.operation_options[0],vet_modes,vet_mode,0);
            set_cycle_state(main_view.operation_options[1],vet_repairs,vet_repair,!vet_mode);
        } else if(selected_operation==DOLLY_DEMO_COPY)
            set_cycle_state(main_view.operation_options[0],copy_modes,copy_mode,0);
    }
    if(!main_view.gadgets_detached) {
        draw_endpoint_frames();
        draw_main_progress();
        draw_main_start_label();
    }
}
/* Rebuild only after draining the window queues, whose IAddress fields
 * can still reference the current gadgets. */
static void sync_action_labels(void) {
    int changed=0;
    if(main_view.window && !main_view.gadgets_detached &&
       (main_cycle_rebuild ||
        (main_view.action_label && strcmp(main_view.action_label,action_label(1)))) &&
       rebuild_gadgets(&main_view,1)) {
        draw_full(&main_view,1);
        changed=1;
    }
    if(sheep_view.window && !sheep_view.gadgets_detached &&
       sheep_view.action_label && strcmp(sheep_view.action_label,action_label(0)) &&
       rebuild_gadgets(&sheep_view,0)) {
        draw_full(&sheep_view,0);
        changed=1;
    }
    if(changed)update_main_actions();
}
static void refresh_main_selection(void) {
    if(!main_view.window)return;
    update_main_status();
    if(main_view.description)
        GT_SetGadgetAttrs(main_view.description,main_view.window,NULL,
                          GTTX_Text,(ULONG)main_view.desc_label,TAG_DONE);
    if(main_view.status)
        GT_SetGadgetAttrs(main_view.status,main_view.window,NULL,
                          GTTX_Text,(ULONG)main_view.status_label,TAG_DONE);
    update_main_actions();
}
static void progress_view_update(unsigned old,int cell_changed) {
    unsigned now=progress_units(GRID_TOTAL),cell;
    ULONG percent=progress_units(100);
    int running=operation.state==DOLLY_OP_RUNNING;
    if(running && percent!=main_view.displayed_percent)draw_main_progress();
    if(!sheep_view.window)return;
    if(old!=now || cell_changed || !running)
        for(cell=old<now?old:now;cell<GRID_TOTAL && cell<=(old>now?old:now);++cell)
            draw_cell(cell,now,running);
    /* For lengthy scans, keep the exact sector count moving even
     * while the integer percentage stays the same. */
    if(++sheep_view.counter_steps>=COUNTER_UPDATE_STEPS ||
       percent!=sheep_view.displayed_percent ||
       (ULONG)operation.errors!=sheep_view.displayed_errors || !running) {
        DemoLayout *d=&sheep_view.l;
        if(!sheep_view.gadgets_detached)
            draw_progress_well(&sheep_view,d->left,d->bar_y,d->right,0);
        update_numbers(&sheep_view);
    }
}
static void toggle_pause(void) {
    quit_blocked=0;
    if(operation.state==DOLLY_OP_RUNNING)(void)dolly_operation_pause(&operation);
    else if(operation.state==DOLLY_OP_PAUSED)(void)dolly_operation_resume(&operation);
    refresh_main_selection();
}
static void ask_stop(void) {
    char reason[580];
    snprintf(reason,sizeof(reason),"%s\nSource: \"%s\"\nDestination: \"%s\"",
             scan_result?"Surface Scan":image_result?"Create Image":
             transfer.restoring?"Restore Image":"Copy",
             selected_source.path[0]?selected_source.path:main_view.source_label,
             scan_result?"-":selected_target.path[0]?selected_target.path:main_view.target_label);
    if(operation.state==DOLLY_OP_RUNNING)(void)dolly_operation_pause(&operation);
    quit_blocked=0;
    refresh_main_selection();
    if(dolly_amiga_role_request(main_view.window,"Dolly - Operation in progress",
                               reason,"Abort","Resume",NULL)==1) {
        if(image.file)dolly_image_cancel(&image);
        else if(transfer.opened)dolly_amiga_transfer_close(&transfer);
        else dolly_surface_close(&surface);
        (void)dolly_operation_cancel(&operation);
    } else (void)dolly_operation_resume(&operation);
    refresh_main_selection();
}

static void process_view(DollyDemoView *v,int main_window,int *quit) {
    struct IntuiMessage *msg;
    int relayout=0,redraw=0,hide=0;
    if(!v->window)return;
    while((msg=GT_GetIMsg(v->window->UserPort))!=NULL) {
        ULONG cls=msg->Class;
        UWORD id=0;
        UWORD code=msg->Code;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_SIZEVERIFY && !v->gadgets_detached &&
           v->gadgets) {
            /* Remove GadTools before Intuition starts interactive sizing,
             * preventing old-size gadgets from flashing in a smaller
             * frame. Reply to SIZEVERIFY immediately afterwards.
             */
            RemoveGList(v->window,v->gadgets,-1);
            v->gadgets_detached=1;
        }
        if(cls==IDCMP_NEWSIZE)relayout=1;
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(v->window);
            GT_EndRefresh(v->window,TRUE);
            redraw=1;
        }
        GT_ReplyIMsg(msg);
        if(main_window) {
            if(cls==IDCMP_CLOSEWINDOW) {
                if(operation_active()) {
                    quit_blocked=1;
                    refresh_main_selection();
                } else *quit=1;
            }
            else if(id==MAIN_SOURCE_ID && !operation_active() && !relayout &&
                    !source_picker.window && !target_picker.window && source_mode_mask()) {
                dolly_picker_close(&target_picker);
                if(medium_modes[0]==DOLLY_MODE_FILE && AslBase) {
                    if(dolly_amiga_pick_file(v->window,&source_picker.result,0))
                        source_picker.changed=1;
                } else if(!dolly_picker_open(&source_picker,public_screen,0,
                                      &selected_source,
                                      medium_modes[0],source_mode_mask()))
                    dolly_picker_close(&source_picker);
                update_main_actions();
            } else if(id==MAIN_TARGET_ID && !operation_active() && !relayout &&
                      !source_picker.window && !target_picker.window &&
                      destination_button_available()) {
                if(recovery_destination()) {
                    if(dolly_amiga_pick_drawer(v->window,recovery_drawer))
                        refresh_main_selection();
                } else if(selected_operation==DOLLY_DEMO_CREATE_IMAGE) {
                    if(dolly_amiga_pick_file(v->window,&selected_target,1)) {
                        selected_target.image_format=selected_image_format();
                        refresh_main_selection();
                    }
                } else if(!dolly_picker_open(&target_picker,public_screen,1,
                                             &selected_target,
                                             medium_modes[1],destination_mode_mask()))
                    dolly_picker_close(&target_picker);
                update_main_actions();
            } else if(cls==IDCMP_GADGETUP &&
                      (id==MAIN_OPERATION_ID || id==MAIN_SOURCE_MODE_ID ||
                       id==MAIN_TARGET_MODE_ID)) {
                if(!main_controls_busy() && !relayout && id==MAIN_OPERATION_ID && code<6u) {
                    selected_operation=code;
                    transfer_result=0;
                    if(selected_operation==DOLLY_DEMO_CREATE_IMAGE)
                        selected_target.image_format=selected_image_format();
                    if(selected_source.kind!=DOLLY_ENDPOINT_NONE &&
                       dolly_mode_allowed(source_mode_mask(),dolly_endpoint_mode(&selected_source)))
                        medium_modes[0]=dolly_endpoint_mode(&selected_source);
                    if(selected_target.kind!=DOLLY_ENDPOINT_NONE &&
                       dolly_mode_allowed(destination_mode_mask(),dolly_endpoint_mode(&selected_target)))
                        medium_modes[1]=dolly_endpoint_mode(&selected_target);
                    relayout=1;
                } else if(!main_controls_busy() && !relayout && id!=MAIN_OPERATION_ID) {
                    unsigned role=id==MAIN_SOURCE_MODE_ID?0u:1u;
                    if(code<v->mode_count[role] && medium_modes[role]!=v->mode_ids[role][code]) {
                        medium_modes[role]=v->mode_ids[role][code];
                        if(!role) {
                            dolly_endpoint_clear(&selected_source);
                            dolly_endpoint_clear(&selected_target);
                        } else dolly_endpoint_clear(&selected_target);
                        relayout=1;
                    }
                }
            } else if(!operation_active() && !relayout &&
                      !source_picker.window && !target_picker.window &&
                      selected_operation==DOLLY_DEMO_CREATE_IMAGE &&
                      ((id==MAIN_IMAGE_FORMAT_ID && code<2u) ||
                       (id==MAIN_COMPRESSION_ID && image_type && code<3u))) {
                if(id==MAIN_IMAGE_FORMAT_ID)image_type=code;
                else compression_type=code;
                selected_target.image_format=selected_image_format();
                refresh_main_selection();
            } else if(id==MAIN_COPY_MODE_ID && code<3u && !operation_active() && !relayout &&
                      !source_picker.window && !target_picker.window &&
                      selected_operation==DOLLY_DEMO_COPY) {
                copy_mode=code;
                refresh_main_selection();
            } else if(!operation_active() && !relayout &&
                      !source_picker.window && !target_picker.window &&
                      selected_operation==DOLLY_DEMO_DISK_VET && code<2u &&
                      (id==MAIN_VET_MODE_ID || (id==MAIN_VET_REPAIR_ID && vet_mode))) {
                if(id==MAIN_VET_MODE_ID)vet_mode=code;
                else vet_repair=code;
                relayout=1;
            } else if(id==MAIN_START_ID && operation_active())toggle_pause();
            else if(id==MAIN_START_ID &&
                      operation.state!=DOLLY_OP_RUNNING &&
                      operation.state!=DOLLY_OP_PAUSED &&
                      !source_picker.window && !target_picker.window &&
                      !source_picker.changed && !target_picker.changed &&
                      !relayout && can_start_operation()) {
                quit_blocked=0;
                if(selected_operation==DOLLY_DEMO_COPY && copy_mode!=0) {
                    dolly_amiga_analyze_copy(v->window,&selected_source,&selected_target,copy_mode==1);
                    refresh_main_selection();
                } else if(selected_operation==DOLLY_DEMO_INVENTORY) {
                    if(!dolly_inventory_view_open(&inventory_view,
                                                   public_screen,&selected_source))
                        dolly_amiga_role_request(v->window,"Dolly","Could not open Sheep Inventory.","OK",NULL,NULL);
                } else {
                    uint64_t total=0;
                    scan_result=(unsigned char)(selected_operation==DOLLY_DEMO_SCAN);
                    image_result=(unsigned char)(selected_operation==DOLLY_DEMO_CREATE_IMAGE);
                    transfer_result=selected_operation==DOLLY_DEMO_COPY ||
                                    selected_operation==DOLLY_DEMO_RESTORE;
                    memset(cell_result,0,sizeof(cell_result));
                    dolly_amiga_log_view_close();
                    dolly_operation_init(&operation);
                    if(scan_result) {
                        DollyWarnings warnings={0};
                        if(dolly_surface_open(&surface,&selected_source,main_view.window,
                                              DOLLY_IO_SOURCE,1,&warnings)) {
                            if(dolly_amiga_guard_read(&surface.mount_guard,main_view.window,
                                   &warnings,&surface.device,surface.first_lba*surface.sector_bytes,
                                   surface.total*surface.sector_bytes,0) &&
                               dolly_warnings_review(main_view.window,&warnings) &&
                               dolly_amiga_read_guard_check(surface.mount_guard))
                                total=surface.total;
                            else {
                                surface.failure="Scan cancelled or source filesystem changed.";
                                dolly_surface_close(&surface);
                            }
                        }
                        dolly_warnings_clear(&warnings);
                    } else if(image_result) {
                        total=dolly_image_open(&image,&selected_source,&selected_target,main_view.window)?
                              image.reader.total:0;
                    } else if(transfer_result) {
                        dolly_inventory_view_close(&inventory_view);
                        transfer_operation=selected_operation;
                        format_endpoint(transfer_from,sizeof(transfer_from),&selected_source);
                        format_endpoint(transfer_to,sizeof(transfer_to),&selected_target);
                        if(dolly_amiga_transfer_open(&transfer,main_view.window,&selected_source,
                              &selected_target,selected_operation==DOLLY_DEMO_RESTORE))
                            total=transfer.job.bytes/transfer.target.sector_bytes;
                    }
                    if(total)(void)dolly_operation_start(&operation,total);
                    if(sheep_view.window) {
                        draw_full(&sheep_view,0);
                        update_numbers(&sheep_view);
                    }
                    refresh_main_selection();
                }
            } else if(id==MAIN_STOP_ID && operation_active()) {
                ask_stop();
            } else if(id==MAIN_DETAILS_ID && operation_active() &&
                      !show_sheep())
                dolly_amiga_role_request(v->window,"Dolly","Could not open Sheep Counter.","OK",NULL,NULL);
        } else if(cls==IDCMP_CLOSEWINDOW || id==SHEEP_HIDE_ID)hide=1;
        if(main_window && (id==MAIN_SETTINGS_ID ||
           (cls==IDCMP_MENUPICK && code==IO_SETTINGS_ITEM)) &&
           io_settings_available())
            dolly_amiga_io_settings(public_screen,&selected_source);
        if(!main_window && id==SHEEP_PAUSE_ID && operation_active())toggle_pause();
        if(!main_window && id==SHEEP_LOG_ID && log_available() &&
           !dolly_amiga_log_view_open(public_screen,&surface,log_outcome()))
            dolly_amiga_role_request(v->window,"Dolly","Could not open the Surface Scan log.","OK",NULL,NULL);
        if(hide)break;
    }
    if(hide) {
        hide_sheep();
        return;
    }
    if(*quit)return;
    if(relayout) {
        if(!rebuild_gadgets(v,main_window)) {
            /* Reattach the previous list if the layout could not be
             * reconstructed. It is no longer safe to leave a detached
             * GadTools list orphaned in an otherwise usable window.
             */
            if(v->gadgets && v->gadgets_detached) {
                AddGList(v->window,v->gadgets,(ULONG)-1,-1,NULL);
                v->gadgets_detached=0;
                RefreshGList(v->gadgets,v->window,NULL,-1);
                GT_RefreshWindow(v->window,NULL);
            }
            return;
        }
        redraw=1;
    }
    if(redraw && !v->gadgets_detached)draw_full(v,main_window);
    if(relayout && main_window)update_main_actions();
}
static int open_main_window(void) {
    ULONG top=(ULONG)public_screen->BarHeight+1ul;
    ULONG max_height=(ULONG)public_screen->Height-top;
    struct RastPort measure=public_screen->RastPort;
    struct TextFont *font=dolly_amiga_font(0);
    DemoLayout layout={0};
    ULONG height=(ULONG)place_main_rows(&layout,font)+
                 top+public_screen->WBorBottom+5u;
    ULONG width;
    SetFont(&measure,font);
    width=(ULONG)main_selector_width(&measure)+32u;
    if(height>max_height)height=max_height;
    if(width<600u)width=600u;
    if(width>(ULONG)public_screen->Width-12u)width=(ULONG)public_screen->Width-12u;
    if(!open_view(&main_view,1,DOLLY_DEMO_BUILD_LABEL,12,top,width,height))return 0;
    settings_menu=CreateMenus(settings_items,TAG_DONE);
    if(settings_menu &&
       (!LayoutMenus(settings_menu,main_view.visual,TAG_DONE) ||
        !SetMenuStrip(main_view.window,settings_menu))) {
        FreeMenus(settings_menu);
        settings_menu=NULL;
    }
    draw_full(&main_view,1);
    refresh_main_selection();
    return 1;
}
static void close_main_window(void) {
    if(settings_menu) {
        ClearMenuStrip(main_view.window);
        FreeMenus(settings_menu);
        settings_menu=NULL;
    }
    destroy_view(&main_view);
}
static int open_main_with_fallback(void) {
    if(open_main_window())return 1;
    close_main_window();
    if(!dolly_amiga_fonts_use_topaz() || !open_main_window())return 0;
    {
        struct EasyStruct request={
            sizeof(struct EasyStruct),0,(STRPTR)"Dolly - Font",
            (STRPTR)"The selected font does not fit this screen. Dolly will use Topaz 8.",
            (STRPTR)"OK"
        };
        EasyRequestArgs(main_view.window,&request,NULL,NULL);
    }
    return 1;
}
static ULONG window_signal(const struct Window *window) {
    return window && window->UserPort ? 1UL<<window->UserPort->mp_SigBit : 0;
}

int main(int argc,char **argv) {
    int quit=0,rc=10;
    const char *prefs_error;
    dolly_operation_init(&operation);
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",36);
    if(!IntuitionBase)return 10;
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",36);
    GadToolsBase=OpenLibrary("gadtools.library",36);
    AslBase=OpenLibrary("asl.library",36); /* Optional file selector. */
    /* libnix startup already owns DOSBase; no extra OpenLibrary. */
    if(!GfxBase || !GadToolsBase)goto cleanup;
    public_screen=LockPubScreen(NULL);
    if(!public_screen)goto cleanup;
    if((ULONG)public_screen->Width<640ul ||
       (ULONG)public_screen->Height<256ul)goto cleanup;
    screen_info=GetScreenDrawInfo(public_screen);
    if(!screen_info)goto cleanup;
    prefs_error=dolly_amiga_prefs_load(argc,argv);
    if(!dolly_amiga_fonts_init(public_screen) || !open_main_with_fallback())
        goto cleanup;
    {
        if(prefs_error) {
            struct EasyStruct request={
                sizeof(struct EasyStruct),0,(STRPTR)"Dolly - Preferences",
                (STRPTR)"%s",(STRPTR)"OK"
            };
            ULONG args[]={(ULONG)prefs_error};
            EasyRequestArgs(main_view.window,&request,NULL,args);
        }
    }
    /* Sheep Counter intentionally remains closed until explicitly shown. */
    while(!quit) {
        if(operation.state==DOLLY_OP_RUNNING)WaitTOF();
        else Wait(window_signal(main_view.window) | window_signal(sheep_view.window) |
                  window_signal(source_picker.window) | window_signal(target_picker.window) |
                  window_signal(inventory_view.window) | dolly_amiga_log_view_signal());
        process_view(&main_view,1,&quit);
        {
            int was_open=source_picker.window!=NULL ||
                         target_picker.window!=NULL;
            dolly_picker_pump(&source_picker);
            dolly_picker_pump(&target_picker);
            if(was_open && !source_picker.window && !target_picker.window)
                update_main_actions();
        }
        if(source_picker.changed) {
            transfer_result=0;
            selected_source=source_picker.result;
            source_file_status[0]=0;
            if(selected_source.kind==DOLLY_ENDPOINT_FILE)
                dolly_amiga_describe_file(&selected_source,source_file_status,sizeof(source_file_status));
            medium_modes[0]=dolly_endpoint_mode(&selected_source);
            source_picker.changed=0;
            if(!dolly_mode_allowed(destination_mode_mask(),
                                   dolly_endpoint_mode(&selected_target)))
                dolly_endpoint_clear(&selected_target);
            if(rebuild_gadgets(&main_view,1))draw_full(&main_view,1);
            refresh_main_selection();
        }
        if(target_picker.changed) {
            transfer_result=0;
            selected_target=target_picker.result;
            medium_modes[1]=dolly_endpoint_mode(&selected_target);
            target_picker.changed=0;
            if(rebuild_gadgets(&main_view,1))draw_full(&main_view,1);
            refresh_main_selection();
        }
        if(sheep_view.window)process_view(&sheep_view,0,&quit);
        dolly_inventory_view_pump(&inventory_view);
        dolly_amiga_log_view_pump();
        if(!quit && transfer.opened && operation.state==DOLLY_OP_RUNNING) {
            unsigned old=progress_units(GRID_TOTAL);
            int finished,was_validating=validating_restore();
            unsigned old_check=transfer_check_percent();
            dolly_amiga_transfer_step(&transfer);
            if(old_check!=transfer_check_percent())refresh_main_selection();
            finished=transfer.job.state!=DOLLY_RESTORE_TRANSFERRING;
            if(transfer.failure && old<GRID_TOTAL)cell_result[old]='X';
            (void)dolly_operation_progress(&operation,
                   transfer.job.processed/transfer.target.sector_bytes,transfer.failure?1u:0u,0);
            if(finished) {
                if(transfer.failure &&
                   transfer.job.source_checked<transfer.job.source_check_bytes) {
                    dolly_amiga_role_request(main_view.window,"Dolly - Restore blocked",
                                             transfer.failure,"OK",NULL,NULL);
                } else if(transfer.failure) {
                    char lba[21],reason[246];
                    uint64_t first=(transfer.target.start+transfer.job.processed)/
                                   transfer.target.sector_bytes;
                    dolly_surface_log_decimal(lba,first);
                    snprintf(reason,sizeof(reason),"%s\nDestination batch starts at LBA %s.\n"
                             "The destination may be partly overwritten.",transfer.failure,lba);
                    dolly_amiga_role_request(main_view.window,"Dolly - Transfer stopped",
                                             reason,"OK",NULL,NULL);
                    dolly_operation_pointer_offer_transfer(main_view.window,&selected_source,
                                                            &selected_target,&transfer);
                }
                dolly_amiga_transfer_close(&transfer);
                (void)dolly_operation_finish(&operation,transfer.failure==NULL);
                refresh_main_selection();
            }
            if(sheep_view.window && was_validating!=validating_restore()) {
                draw_full(&sheep_view,0);
                update_numbers(&sheep_view);
            }
            progress_view_update(old,transfer.failure!=NULL);
        } else if(!quit && image.file && operation.state==DOLLY_OP_RUNNING) {
            unsigned old=progress_units(GRID_TOTAL);
            dolly_image_step(&image);
            if(image.reader.processed<operation.completed_blocks)
                (void)dolly_operation_rewind(&operation,image.reader.processed);
            else
                (void)dolly_operation_progress(&operation,image.reader.processed,0,0);
            if(image.failure && image.file) {
                dolly_operation_pointer_offer(main_view.window,&selected_source,&selected_target,
                                              &image.reader,&image);
                dolly_image_cancel(&image);
            }
            if(!image.file) {
                (void)dolly_operation_finish(&operation,image.failure==NULL);
                refresh_main_selection();
            }
            progress_view_update(old,0);
        } else if(!quit && surface.source && operation.state==DOLLY_OP_RUNNING) {
            unsigned old=progress_units(GRID_TOTAL);
            uint32_t errors=surface.errors;
            unsigned retry_pending=surface.retry_pending;
            int cell_changed=0;
            dolly_surface_step(&surface);
            if(old<GRID_TOTAL && cell_result[old]!='X') {
                char result=cell_result[old];
                if(surface.errors!=errors)result='X';
                else if(retry_pending && !surface.retry_pending)result='R';
                cell_changed=result!=cell_result[old];
                cell_result[old]=result;
            }
            (void)dolly_operation_progress(&operation,surface.processed,
                                           surface.errors,surface.retries);
            if(surface.failure || surface.processed==surface.total) {
                (void)dolly_operation_finish(&operation,surface.failure==NULL);
                dolly_surface_close(&surface);
                refresh_main_selection();
            }
            progress_view_update(old,cell_changed);
        }
        if(!quit && dolly_amiga_fonts_changed()) {
            int counter_open=sheep_view.window!=NULL;
            hide_sheep();
            close_main_window();
            dolly_amiga_fonts_apply();
            if(!open_main_with_fallback())goto cleanup;
            if(counter_open)show_sheep();
            dolly_picker_refresh_font(&source_picker);
            dolly_picker_refresh_font(&target_picker);
            dolly_inventory_view_refresh_font(&inventory_view);
            dolly_amiga_fonts_release_previous();
        }
        if(!quit)sync_action_labels();
    }
    rc=0;
cleanup:
    dolly_amiga_transfer_close(&transfer);
    dolly_image_cancel(&image);
    dolly_amiga_log_view_close();
    dolly_surface_close(&surface);
    dolly_surface_log_clear(&surface.log);
    dolly_inventory_view_close(&inventory_view);
    dolly_picker_close(&source_picker);
    dolly_picker_close(&target_picker);
    hide_sheep();
    dolly_amiga_io_settings_clear();
    close_main_window();
    dolly_amiga_fonts_close();
    if(screen_info && public_screen)
        FreeScreenDrawInfo(public_screen,screen_info);
    if(public_screen)UnlockPubScreen(NULL,public_screen);
    if(AslBase)CloseLibrary(AslBase);
    if(GadToolsBase)CloseLibrary(GadToolsBase);
    if(GfxBase)CloseLibrary((struct Library *)GfxBase);
    if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
