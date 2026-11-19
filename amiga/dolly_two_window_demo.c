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
#include "dolly_amiga_restore_confirm.h"
#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
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
#define MAIN_SHOW_ID 201u
#define MAIN_QUIT_ID 202u
#define SHEEP_HIDE_ID 203u
#define MAIN_COUNT_ID 204u
#define SHEEP_COUNT_ID 205u
#define SHEEP_ERRORS_ID 206u
#define SHEEP_PERCENT_ID 207u
#define SHEEP_LOG_ID 220u
#define SHEEP_PAUSE_ID 221u
#define MAIN_START_ID 208u
#define MAIN_DEVICE_ID 209u
#define MAIN_OPERATION_ID 210u
#define MAIN_SOURCE_MODE_ID 220u
#define MAIN_TARGET_MODE_ID 221u
#define MAIN_IMAGE_FORMAT_ID 222u
#define MAIN_COMPRESSION_ID 223u
#define MAIN_COPY_MODE_ID 224u
#define MAIN_STATUS_ID 211u
#define MAIN_SOURCE_ID 212u
#define MAIN_TARGET_ID 213u
#define MAIN_DESC_ID 214u
#define MAIN_SOURCE_TEXT_ID 215u
#define MAIN_TARGET_TEXT_ID 216u
#define MAIN_SETTINGS_ID 217u
#define MAIN_STOP_ID 218u
#define MAIN_PLACEHOLDER_ID 219u
#define DOLLY_DEMO_BUILD_LABEL "Dolly Alpha 3"
#define MAIN_CONTROLS_OFFSET 47
#define MAIN_PROGRESS_OFFSET 85
#define MAIN_STATUS_OFFSET 107
#define MAIN_GADGET_HEIGHT 15
#define MAIN_COLUMN_GAP 6
#define MAIN_STATUS_BEVEL_HEIGHT 19

typedef struct DemoLayout {
    LONG left, right, top, bottom;
    LONG endpoint_button_y,endpoint_frame_y,main_operation_y;
    LONG main_desc_y, main_status_y, main_count_y;
    LONG progress_x, progress_w, stop_x, quit_x, stop_quit_w;
    LONG selector_x[3],selector_w[3],selector_y,selector_h;
    LONG pad_x, pad_y;
    LONG controls_y, bar_y, bar_right, percent_x, percent_w;
    LONG grid_x, grid_y, cell_w, cell_h, gap_x, gap_y;
    LONG grid_w, grid_h, grid_cols;
    LONG number_w, error_x, log_x, log_w, hide_x, hide_w, pause_x;
} DemoLayout;
typedef struct DollyDemoView {
    struct Window *window;
    struct Gadget *gadgets;
    struct Gadget *show, *settings, *placeholder, *stop, *quit;
    struct Gadget *start, *hide, *log, *pause, *count, *errors, *percent;
    const char *action_label;
    struct Gadget *device,*target_button,*source_text,*target_text;
    struct Gadget *description,*selectors[3],*operation_options[2],*status;
    STRPTR mode_labels[2][5];
    DollyEndpointMode mode_ids[2][4];
    unsigned mode_count[2],mode_active[2];
    char source_label[72],target_label[72],desc_label[96];
    char status_label[64];
    char block_label[40];
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
static struct TextFont *marker_font;
static DollyOperationSession operation;
static DollySurfaceScan surface;
static unsigned char scan_result,image_result;
static DollyAmigaImage image;
static char cell_result[GRID_TOTAL];
static DollyDemoView main_view, sheep_view;
/* SYNTHETIC device tree. This demo never opens physical media. */
static DollyEndpoint selected_source,selected_target;
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
static unsigned image_type,compression_type,copy_mode;
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

static DollyEndpointMode medium_modes[2]={DOLLY_MODE_PHYSICAL,DOLLY_MODE_PHYSICAL};
static STRPTR no_destination_choices[]={(STRPTR)"Not Required",NULL};
static STRPTR awaiting_source_choices[]={(STRPTR)"Select a source",NULL};
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
static LONG main_selector_extra(const struct TextFont *font) {
    return font->tf_YSize+3*selector_height(font)-3;
}
static LONG main_selector_width(struct RastPort *rp) {
    LONG width=0;
    unsigned i;
    static const char *const extra_labels[]={
        "Select a source","Not Required","Sheep Counter","Destination...",
        "Sector-to-sector","Clone and resize","Not compressed"
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
    return 3*(width+30)+2*MAIN_COLUMN_GAP;
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
    return (selected_operation!=DOLLY_DEMO_COPY || copy_mode==0) &&
           dolly_demo_can_start((DollyDemoOperation)selected_operation,
                                &selected_source,&selected_target);
}
static int operation_active(void) {
    return operation.state==DOLLY_OP_RUNNING || operation.state==DOLLY_OP_PAUSED;
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
static void update_main_status(void) {
    int need=dolly_demo_needs_target((DollyDemoOperation)selected_operation);
    int can=dolly_demo_can_start((DollyDemoOperation)selected_operation,
                                &selected_source,&selected_target);
    text_copy(main_view.desc_label,sizeof(main_view.desc_label),
              selected_operation==DOLLY_DEMO_COPY && copy_mode?
              (copy_mode==1?"Smart Copy is planned.":"Clone and resize is planned."):
              demo_descriptions[selected_operation]);
    if(quit_blocked && operation_active())
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Operation in progress. Cannot quit.");
    else if(operation.state==DOLLY_OP_PAUSED)
        text_copy(main_view.status_label,sizeof(main_view.status_label),"Operation paused.");
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
    else if(selected_operation==DOLLY_DEMO_INVENTORY && can)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Ready for read-only Inventory.");
    else if(selected_operation==DOLLY_DEMO_DISK_VET)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
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
    else if(selected_operation==DOLLY_DEMO_COPY && copy_mode)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Selected copy mode is not available in this build.");
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
    if(need)
        format_endpoint(main_view.target_label,sizeof(main_view.target_label),&selected_target);
    else text_copy(main_view.target_label,sizeof(main_view.target_label),"Not Required");
    if(main_view.window) {
        DemoLayout *d=&main_view.l;
        clip_main(main_view.source_label,d->selector_w[1]-8);
        clip_main(main_view.target_label,d->selector_w[2]-8);
        clip_main(main_view.desc_label,d->right-d->left);
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
static void append_char(char **out,char *limit,char c) {
    if(*out<limit) *(*out)++=c;
}
static int log_available(void) {
    return !image_result && surface.source_path[0] && !surface.source;
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
static ULONG progress_units(ULONG units) {
    return operation.total_blocks?
           (ULONG)(operation.completed_blocks*units/operation.total_blocks):0;
}
static void update_labels(DollyDemoView *v) {
    char *p=v->block_label;
    char *limit=v->block_label+sizeof(v->block_label)-1;
    unsigned long current=(unsigned long)operation.completed_blocks;
    unsigned long total=(unsigned long)operation.total_blocks;
    unsigned long percent=progress_units(100);
    v->displayed_percent=percent;
    if(v==&main_view) {
        append_u32(&p,limit,percent>100ul?100ul:percent);
        append_char(&p,limit,'%');
    } else {
        append_u32(&p,limit,current);
        append_char(&p,limit,' ');
        append_char(&p,limit,'/');
        append_char(&p,limit,' ');
        append_u32(&p,limit,total);
    }
    *p='\0';
    p=v->percent_label;
    limit=v->percent_label+sizeof(v->percent_label)-1;
    append_u32(&p,limit,percent>100ul?100ul:percent);
    append_char(&p,limit,'%');
    *p='\0';
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
           (span==best_span && unused>=best_unused))continue;
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
    LONG avail_w,avail_h,grid_bottom;
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
    if(main_window) {
        LONG extra=main_selector_extra(w->RPort->Font);
        LONG column_width=(avail_w-2*MAIN_COLUMN_GAP)/3;
        LONG footer_extra=selector_height(w->RPort->Font)-MAIN_GADGET_HEIGHT;
        LONG progress=TextLength(w->RPort,(STRPTR)"Progress:",9)+10;
        unsigned column;
        if(avail_w<320 || avail_w<main_selector_width(w->RPort) ||
           avail_h<MAIN_STATUS_OFFSET+MAIN_STATUS_BEVEL_HEIGHT+extra+footer_extra)
            return 0;
        d.main_operation_y=d.top+3;
        d.selector_y=d.main_operation_y+w->RPort->Font->tf_YSize+4;
        d.selector_h=selector_height(w->RPort->Font);
        for(column=0;column<3u;++column) {
            d.selector_x[column]=d.left+(LONG)column*(column_width+MAIN_COLUMN_GAP);
            d.selector_w[column]=column_width;
        }
        d.endpoint_button_y=d.selector_y+d.selector_h+4;
        d.endpoint_frame_y=d.endpoint_button_y+d.selector_h+4;
        d.main_desc_y=d.top+23+extra;
        d.controls_y=d.top+MAIN_CONTROLS_OFFSET+extra;
        d.main_count_y=d.top+MAIN_PROGRESS_OFFSET+extra+footer_extra;
        d.main_status_y=d.top+MAIN_STATUS_OFFSET+extra+footer_extra;
        d.progress_x=d.left+(progress>86?progress:86);
        d.progress_w=66;
        /* Keep 320px client layouts viable: shorten both buttons
         * together instead of raising the minimum width.
         */
        d.stop_quit_w=avail_w<360?70:90;
        d.quit_x=d.right-d.stop_quit_w+1;
        d.stop_x=d.quit_x-d.stop_quit_w-6;
        if(d.stop_x<d.progress_x+d.progress_w+12+
           (LONG)TextLength(w->RPort,(STRPTR)DOLLY_DEMO_BUILD_LABEL,
                            (ULONG)strlen(DOLLY_DEMO_BUILD_LABEL)))return 0;
        d.number_w=avail_w/3;
        if(d.number_w>150)d.number_w=150;
    } else {
        if(avail_w<248 || avail_h<140)return 0;
        d.hide_w=sheep_button_width(w->RPort,hide_label);
        d.hide_x=d.right-d.hide_w+1;
        d.log_w=sheep_button_width(w->RPort,show_log_label);
        d.log_x=d.hide_x-d.log_w-6;
        d.pause_x=d.log_x-d.log_w-6;
        d.controls_y=d.bottom-15;
        d.bar_y=d.controls_y-26;
        d.percent_w=46;
        d.percent_x=d.right-d.percent_w+1;
        d.bar_right=d.percent_x-7;
        grid_bottom=d.bar_y-7;
        if(!fit_grid(&d,avail_w-6,grid_bottom-(d.top+19)))return 0;
        d.grid_x=d.left+(avail_w-d.grid_w)/2;
        d.grid_y=d.top+19+(grid_bottom-(d.top+19)-d.grid_h)/2;
        d.number_w=(d.pause_x-d.left-12)/2;
        d.error_x=d.left+d.number_w+6;
        if(d.number_w<80)return 0;
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
    ng.ng_TextAttr=public_screen->Font;
    ng.ng_LeftEdge=(WORD)x;
    ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)width;
    ng.ng_Height=(id==MAIN_SHOW_ID || id==MAIN_SETTINGS_ID || id==MAIN_PLACEHOLDER_ID ||
                  id==MAIN_SOURCE_ID || id==MAIN_TARGET_ID)?
                 (WORD)v->l.selector_h:MAIN_GADGET_HEIGHT;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)title;
    return CreateGadget(BUTTON_KIND,last,&ng,
                        GA_Disabled,(ULONG)(
                          id==MAIN_PLACEHOLDER_ID ||
                          (id==SHEEP_LOG_ID && !log_available()) ||
                          (id==MAIN_STOP_ID &&
                           !operation_active()) ||
                          (id==MAIN_SHOW_ID && !counter_available()) ||
                          (id==MAIN_START_ID && !operation_active() && !can_start_operation()) ||
                          (id==SHEEP_PAUSE_ID && !operation_active()) ||
                          (id==MAIN_TARGET_ID &&
                           (!dolly_demo_needs_target(
                             (DollyDemoOperation)selected_operation) ||
                            !destination_mode_mask()))),
                        TAG_DONE);
}
static struct Gadget *make_number(struct Gadget *last,DollyDemoView *v,
                                  UWORD id,LONG x,LONG y,LONG width,
                                  ULONG value) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=public_screen->Font;
    ng.ng_LeftEdge=(WORD)x;
    ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)width;
    ng.ng_Height=15;
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
    ng.ng_TextAttr=public_screen->Font;
    ng.ng_LeftEdge=(WORD)x;
    ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)width;
    ng.ng_Height=(id==MAIN_SOURCE_TEXT_ID || id==MAIN_TARGET_TEXT_ID)?
                 (WORD)v->l.selector_h:MAIN_GADGET_HEIGHT;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)value,
                        GTTX_Border,border,
                        GTTX_Clipped,TRUE,TAG_DONE);
}
static struct Gadget *make_selector(struct Gadget *last,DollyDemoView *v,
                                    UWORD id,unsigned column,LONG y,STRPTR *choices,
                                    unsigned active,int disabled) {
    struct NewGadget ng={0};
    LONG x=v->l.selector_x[column];
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=public_screen->Font;
    ng.ng_LeftEdge=(WORD)x;
    ng.ng_TopEdge=(WORD)y;
    ng.ng_Width=(WORD)v->l.selector_w[column];
    ng.ng_Height=(WORD)v->l.selector_h;
    ng.ng_GadgetID=id;
    return CreateGadget(CYCLE_KIND,last,&ng,
                        GTCY_Labels,(ULONG)choices,GTCY_Active,(ULONG)active,
                        GA_Disabled,(ULONG)disabled,
                        TAG_DONE);
}
static int rebuild_gadgets(DollyDemoView *v,int main_window) {
    struct Gadget *last;
    DemoLayout *d=&v->l;
    if(!calculate_layout(v,main_window))return 0;
    update_labels(v);
    if(v->gadgets) {
        /* IDCMP_SIZEVERIFY already detached the list before sizing.
         * Never remove the same gadgets twice.
         */
        if(!v->gadgets_detached)
            RemoveGList(v->window,v->gadgets,-1);
        FreeGadgets(v->gadgets);
        v->gadgets=NULL;
        v->show=NULL;v->settings=NULL;v->placeholder=NULL;
        v->stop=NULL;v->quit=NULL;v->start=NULL;v->hide=NULL;
        v->count=NULL;v->errors=NULL;v->percent=NULL;v->log=NULL;v->pause=NULL;
        v->device=NULL;v->target_button=NULL;
        v->source_text=NULL;v->target_text=NULL;
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
        last=make_text(last,v,MAIN_SOURCE_TEXT_ID,d->selector_x[1],d->endpoint_frame_y,
                       d->selector_w[1],v->source_label,TRUE);
        if(!last)return 0;
        v->source_text=last;
        last=make_button(last,v,MAIN_TARGET_ID,d->selector_x[2],d->endpoint_button_y,
                         d->selector_w[2],"Destination...");
        if(!last)return 0;
        v->target_button=last;
        last=make_text(last,v,MAIN_TARGET_TEXT_ID,d->selector_x[2],d->endpoint_frame_y,
                       d->selector_w[2],v->target_label,TRUE);
        if(!last)return 0;
        v->target_text=last;
        last=make_selector(last,v,MAIN_OPERATION_ID,0,d->selector_y,
                           demo_operations,selected_operation,0);
        if(!last)return 0;
        v->selectors[0]=last;
        for(role=0;role<2u;++role) {
            STRPTR *choices=v->mode_labels[role];
            if(!v->mode_count[role])
                choices=role && !dolly_demo_needs_target((DollyDemoOperation)selected_operation)?
                        no_destination_choices:awaiting_source_choices;
            last=make_selector(last,v,role?MAIN_TARGET_MODE_ID:MAIN_SOURCE_MODE_ID,
                               role+1u,d->selector_y,choices,
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
        }
        last=make_text(last,v,MAIN_DESC_ID,d->left,d->main_desc_y,
                       d->right-d->left+1,v->desc_label,FALSE);
        if(!last)return 0;
        v->description=last;
        last=make_button(last,v,MAIN_SHOW_ID,d->selector_x[0],d->controls_y+19,
                         d->selector_w[0],"Sheep Counter");
        if(!last)return 0;
        v->show=last;
        last=make_button(last,v,MAIN_SETTINGS_ID,
                         d->selector_x[1],d->controls_y+19,
                         d->selector_w[1],"Settings");
        if(!last)return 0;
        v->settings=last;
        last=make_button(last,v,MAIN_PLACEHOLDER_ID,
                         d->selector_x[2],d->controls_y+19,
                         d->selector_w[2],"");
        if(!last)return 0;
        v->placeholder=last;
        last=make_button(last,v,MAIN_START_ID,d->left,
                         d->controls_y,d->right-d->left+1,action_label(1));
        if(!last)return 0;
        v->start=last;
        last=make_text(last,v,MAIN_COUNT_ID,d->progress_x,d->main_count_y,
                       d->progress_w,v->block_label,TRUE);
        if(!last)return 0;
        v->count=last;
        last=make_button(last,v,MAIN_STOP_ID,
                         d->stop_x,d->main_count_y,d->stop_quit_w,"Stop");
        if(!last)return 0;
        v->stop=last;
        last=make_button(last,v,MAIN_QUIT_ID,
                         d->quit_x,d->main_count_y,d->stop_quit_w,"Quit");
        if(!last)return 0;
        v->quit=last;
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
        last=make_text(last,v,SHEEP_PERCENT_ID,
                       d->percent_x,d->bar_y-4,d->percent_w,
                       v->percent_label,FALSE);
        if(!last)return 0;
        v->percent=last;
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
    v->show=NULL;v->settings=NULL;v->placeholder=NULL;
    v->stop=NULL;v->quit=NULL;v->start=NULL;v->hide=NULL;
    v->count=NULL;v->errors=NULL;v->percent=NULL;
    v->device=NULL;v->target_button=NULL;
    v->source_text=NULL;v->target_text=NULL;
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
        ULONG status_bottom=(ULONG)(MAIN_STATUS_OFFSET-2+
                                    MAIN_STATUS_BEVEL_HEIGHT);
        ULONG progress_bottom=(ULONG)(MAIN_PROGRESS_OFFSET+
                                      MAIN_GADGET_HEIGHT);
        ULONG buttons_bottom=(ULONG)(MAIN_CONTROLS_OFFSET+19+
                                     MAIN_GADGET_HEIGHT);
        m.main_content_height=status_bottom;
        if(m.main_content_height<progress_bottom)
            m.main_content_height=progress_bottom;
        if(m.main_content_height<buttons_bottom)
            m.main_content_height=buttons_bottom;
        /* Same visual breathing room below as above the controls. */
        m.main_content_height+=3u+(ULONG)main_selector_extra(rp->Font)+
            (ULONG)(selector_height(rp->Font)-MAIN_GADGET_HEIGHT);
        m.main_build_label_width=(ULONG)TextLength(rp,
            (STRPTR)DOLLY_DEMO_BUILD_LABEL,
            (ULONG)strlen(DOLLY_DEMO_BUILD_LABEL));
        {
            static const char choice[]="Restore a file image to a destination.";
            ULONG progress=(ULONG)TextLength(rp,(STRPTR)"Progress:",9)+10u;
            ULONG b=(ULONG)TextLength(rp,(STRPTR)choice,
                                       (ULONG)strlen(choice));
            m.heading_width=(ULONG)main_selector_width(rp);
            if(m.heading_width<b)m.heading_width=b;
            if(progress>86u)m.main_build_label_width+=progress-86u;
        }
        m.show_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)"Sheep Counter",13);
        m.quit_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)"Quit",4);
    } else {
        ULONG digit_width=0;
        char digit;
        for(digit='0';digit<='9';++digit) {
            ULONG width=(ULONG)TextLength(rp,(STRPTR)&digit,1);
            if(width>digit_width)digit_width=width;
        }
        ULONG scan_width=(ULONG)TextLength(rp,(STRPTR)counter_heading,
                sizeof(counter_heading)-1);
        ULONG image_width=(ULONG)TextLength(rp,(STRPTR)image_heading,
                sizeof(image_heading)-1);
        m.heading_width=(scan_width>image_width?scan_width:image_width)+8u+10u*digit_width+
                (ULONG)TextLength(rp,(STRPTR)grid_legend_prefix,
                sizeof(grid_legend_prefix)-1)+
                (ULONG)TextLength(rp,(STRPTR)grid_legend_suffix,
                sizeof(grid_legend_suffix)-1);
        m.hide_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)hide_label,sizeof(hide_label)-1);
        m.log_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)show_log_label,sizeof(show_log_label)-1);
    }
    minimum=dolly_gui_measure_minimum(
        main_window?DOLLY_GUI_WINDOW_MAIN:DOLLY_GUI_WINDOW_SHEEP,m);
    if(!minimum.valid)return 0;
    if(!main_window) {
        LONG pixel_x,pixel_y,span;
        ULONG width,height;
        grid_pixel_size(&pixel_x,&pixel_y);
        span=8*(pixel_x>pixel_y?pixel_x:pixel_y);
        width=m.border_left+m.border_right+2*m.padding_x+6+
              GRID_COLS*((span+pixel_x/2)/pixel_x)+
              (GRID_COLS-1)*grid_gap(pixel_x,pixel_y);
        height=m.border_top+m.border_bottom+2*m.padding_y+68+
               GRID_ROWS*((span+pixel_y/2)/pixel_y)+
               (GRID_ROWS-1)*grid_gap(pixel_y,pixel_x);
        if(minimum.outer_width<width)minimum.outer_width=width;
        if(minimum.outer_height<height)minimum.outer_height=height;
    }
    if(minimum.outer_width>(ULONG)w->Width ||
       minimum.outer_height>(ULONG)w->Height)return 0;
    if(main_window)
        /* Keep current height fixed; allow only horizontal resizing. */
        return WindowLimits(w,(LONG)minimum.outer_width,
                            (LONG)w->Height,0,(LONG)w->Height)!=FALSE;
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
    if(!set_content_minimum(v,main_window)) {
        puts("WINDOW FAIL: measured minimum does not fit");
        return 0;
    }
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
    /* Keep both formatted strings in view-owned storage. */
    update_labels(v);
    GT_SetGadgetAttrs(v->count,v->window,NULL,
                      GTTX_Text,(ULONG)v->block_label,TAG_DONE);
    if(v->percent)
        GT_SetGadgetAttrs(v->percent,v->window,NULL,
                          GTTX_Text,(ULONG)v->percent_label,TAG_DONE);
    v->counter_steps=0;
    if(v->errors && v->displayed_errors!=(ULONG)operation.errors) {
        GT_SetGadgetAttrs(v->errors,v->window,NULL,
                          GTNM_Number,(ULONG)operation.errors,TAG_DONE);
    }
    v->displayed_errors=(ULONG)operation.errors;
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
    label(main_view.window,d->left,d->main_count_y+11,"Progress:");
    /* Build label occupies only the gap between the progress well
     * and Stop. Omit it when a minimum-width window cannot fit it;
     * never paint text underneath an interactive gadget.
     */
    {
        struct RastPort *rp=main_view.window->RPort;
        LONG from=d->progress_x+d->progress_w+6;
        LONG to=d->stop_x-6;
        LONG tw=(LONG)TextLength(rp,(STRPTR)DOLLY_DEMO_BUILD_LABEL,
                                 (ULONG)strlen(DOLLY_DEMO_BUILD_LABEL));
        if(to-from>=tw) {
            SetDrMd(rp,JAM1);
            SetAPen(rp,system_pen(SHINEPEN));
            Move(rp,from+(to-from-tw)/2,d->main_count_y+11);
            Text(rp,(STRPTR)DOLLY_DEMO_BUILD_LABEL,
                 (ULONG)strlen(DOLLY_DEMO_BUILD_LABEL));
        }
    }
    tags[0].ti_Tag=GT_VisualInfo;
    tags[0].ti_Data=(ULONG)main_view.visual;
    tags[1].ti_Tag=GTBB_Recessed;
    tags[1].ti_Data=TRUE;
    tags[2].ti_Tag=TAG_DONE;
    tags[2].ti_Data=0;
    DrawBevelBoxA(main_view.window->RPort,
                  d->left,d->main_status_y-2,
                  d->right-d->left+1,19,tags);
}
static void draw_marker(struct RastPort *rp,LONG x,LONG y,
                        LONG width,LONG height,char marker,ULONG pen) {
    struct TextFont *saved=rp->Font;
    UBYTE mode=rp->DrawMode;
    LONG glyph_width;
    if(!marker_font)return;
    SetFont(rp,marker_font);
    glyph_width=(LONG)TextLength(rp,(STRPTR)&marker,1);
    SetDrMd(rp,JAM1);
    SetAPen(rp,system_pen(pen));
    Move(rp,x+(width-glyph_width)/2,
         y+(height-(LONG)marker_font->tf_YSize)/2+
           (LONG)marker_font->tf_Baseline);
    Text(rp,(STRPTR)&marker,1);
    SetDrMd(rp,mode);
    if(saved)SetFont(rp,saved);
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
/* The bezel is system-supplied and recessed just like the grid's frame.
 * The fill has no drawn outline: background / FILLPEN only in the well.
 */
static void draw_progress_bar(int full) {
    DemoLayout *d=&sheep_view.l;
    struct RastPort *rp=sheep_view.window->RPort;
    struct TagItem tags[3];
    LONG interior_w=d->bar_right-d->left-3;
    LONG filled=(LONG)progress_units((ULONG)interior_w);
    LONG previous=sheep_view.bar_filled;
    if(full) {
        tags[0].ti_Tag=GT_VisualInfo;
        tags[0].ti_Data=(ULONG)sheep_view.visual;
        tags[1].ti_Tag=GTBB_Recessed;
        tags[1].ti_Data=TRUE;
        tags[2].ti_Tag=TAG_DONE;
        tags[2].ti_Data=0;
        DrawBevelBoxA(rp,d->left,d->bar_y,d->bar_right-d->left+1,
                      10,tags);
        fill(rp,system_pen(BACKGROUNDPEN),
             d->left+2,d->bar_y+2,d->bar_right-2,d->bar_y+7);
        previous=0;
    }
    if(filled>previous)
        fill(rp,system_pen(FILLPEN),d->left+2+previous,d->bar_y+2,
             d->left+1+filled,d->bar_y+7);
    else if(filled<previous)
        fill(rp,system_pen(BACKGROUNDPEN),d->left+2+filled,d->bar_y+2,
             d->left+1+previous,d->bar_y+7);
    sheep_view.bar_filled=filled;
}
static void draw_sheep_static(void) {
    DemoLayout *d=&sheep_view.l;
    struct RastPort *rp;
    struct TagItem tags[3];
    unsigned i,completed=progress_units(GRID_TOTAL);
    const char *heading=image_result?image_heading:counter_heading;
    if(!sheep_view.window)return;
    rp=sheep_view.window->RPort;
    label(sheep_view.window,d->left,d->top+10,heading);
    if((scan_result || image_result) && operation.total_blocks) {
        char legend[sizeof(grid_legend_prefix)+10+sizeof(grid_legend_suffix)];
        char *p=legend+sizeof(grid_legend_prefix)-1;
        LONG width;
        /* Proportional cells may differ by one sector. Show the upper
         * bound, rounded up so that even a small scan shows one sector. */
        uint64_t total=operation.total_blocks;
        memcpy(legend,grid_legend_prefix,sizeof(grid_legend_prefix)-1);
        append_u32(&p,p+10,(unsigned long)(total/GRID_TOTAL+
                   (total%GRID_TOTAL!=0)));
        memcpy(p,grid_legend_suffix,sizeof(grid_legend_suffix));
        width=(LONG)TextLength(rp,(STRPTR)legend,(ULONG)strlen(legend));
        LONG right=d->grid_x+d->grid_w+1;
        LONG heading_width=TextLength(rp,(STRPTR)heading,strlen(heading));
        if(right-width<d->left+heading_width+8)right=d->right+1;
        label(sheep_view.window,right-width,d->top+10,legend);
    }
    tags[0].ti_Tag=GT_VisualInfo;
    tags[0].ti_Data=(ULONG)sheep_view.visual;
    tags[1].ti_Tag=GTBB_Recessed;
    tags[1].ti_Data=TRUE;
    tags[2].ti_Tag=TAG_DONE;
    tags[2].ti_Data=0;
    DrawBevelBoxA(rp,d->grid_x-2,d->grid_y-2,
                  d->grid_w+4,d->grid_h+4,tags);
    fill(rp,system_pen(BACKGROUNDPEN),d->grid_x,d->grid_y,
         d->grid_x+d->grid_w-1,d->grid_y+d->grid_h-1);
    for(i=0;i<GRID_TOTAL;++i)
        draw_cell(i,completed,operation.state==DOLLY_OP_RUNNING);
    draw_progress_bar(1);
    label(sheep_view.window,d->left,d->controls_y-3,"Blocks:");
    label(sheep_view.window,d->error_x,d->controls_y-3,"Errors:");
}
static void draw_full(DollyDemoView *v,int main_window) {
    clear_client(v);
    if(main_window)draw_main_static();
    else draw_sheep_static();
    RefreshGList(v->gadgets,v->window,NULL,-1);
    GT_RefreshWindow(v->window,NULL);
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
    if(main_view.placeholder)
        GT_SetGadgetAttrs(main_view.placeholder,main_view.window,NULL,
                          GA_Disabled,TRUE,TAG_DONE);
    if(main_view.stop)
        GT_SetGadgetAttrs(main_view.stop,main_view.window,NULL,
                          GA_Disabled,(ULONG)!active,TAG_DONE);
    if(main_view.show)
        GT_SetGadgetAttrs(main_view.show,main_view.window,NULL,
                          GA_Disabled,(ULONG)!counter_available(),TAG_DONE);
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
                          GA_Disabled,(ULONG)(active || !destination_mode_mask() ||
                          source_picker.window || target_picker.window),TAG_DONE);
    {
        int busy=active || source_picker.window || target_picker.window;
        unsigned i;
        for(i=0;i<3u;++i)if(main_view.selectors[i])
            GT_SetGadgetAttrs(main_view.selectors[i],main_view.window,NULL,
                              GA_Disabled,(ULONG)(busy ||
                                                 (i && !main_view.mode_count[i-1u])),TAG_DONE);
        for(i=0;i<2u;++i)if(main_view.operation_options[i])
            GT_SetGadgetAttrs(main_view.operation_options[i],main_view.window,NULL,
                              GA_Disabled,(ULONG)(busy || (i && !image_type)),TAG_DONE);
    }
}
/* Rebuild only after draining the window queues, whose IAddress fields
 * can still reference the current gadgets. */
static void sync_action_labels(void) {
    int changed=0;
    if(main_view.window && !main_view.gadgets_detached &&
       main_view.action_label && strcmp(main_view.action_label,action_label(1)) &&
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
    if(main_view.source_text)
        GT_SetGadgetAttrs(main_view.source_text,main_view.window,NULL,
                          GTTX_Text,(ULONG)main_view.source_label,TAG_DONE);
    if(main_view.target_text)
        GT_SetGadgetAttrs(main_view.target_text,main_view.window,NULL,
                          GTTX_Text,(ULONG)main_view.target_label,TAG_DONE);
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
    if(percent!=main_view.displayed_percent || !running)
        update_numbers(&main_view);
    if(!sheep_view.window)return;
    if(old!=now || cell_changed || !running)
        for(cell=old;cell<=now && cell<GRID_TOTAL;++cell)
            draw_cell(cell,now,running);
    /* For lengthy scans, keep the exact sector count moving even
     * while the integer percentage stays the same. */
    if(++sheep_view.counter_steps>=COUNTER_UPDATE_STEPS ||
       percent!=sheep_view.displayed_percent ||
       (ULONG)operation.errors!=sheep_view.displayed_errors || !running) {
        draw_progress_bar(0);
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
             scan_result?"Surface Scan":image_result?"Create Image":"Copy",
             selected_source.path[0]?selected_source.path:main_view.source_label,
             scan_result?"-":selected_target.path[0]?selected_target.path:main_view.target_label);
    if(operation.state==DOLLY_OP_RUNNING)(void)dolly_operation_pause(&operation);
    quit_blocked=0;
    refresh_main_selection();
    if(dolly_amiga_role_request(main_view.window,"Dolly - Operation in progress",
                               reason,"Abort","Resume",NULL)==1) {
        if(image.file)dolly_image_cancel(&image);
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
            if(cls==IDCMP_CLOSEWINDOW || id==MAIN_QUIT_ID) {
                if(operation_active()) {
                    quit_blocked=1;
                    refresh_main_selection();
                } else *quit=1;
            }
            else if(id==MAIN_SOURCE_ID && !operation_active() && !relayout &&
                    !source_picker.window && !target_picker.window && source_mode_mask()) {
                dolly_picker_close(&target_picker);
                if(!dolly_picker_open(&source_picker,public_screen,0,
                                      &selected_source,
                                      medium_modes[0],DOLLY_MODE_BIT(medium_modes[0])))
                    dolly_picker_close(&source_picker);
                update_main_actions();
            } else if(id==MAIN_TARGET_ID && !operation_active() && !relayout &&
                      !source_picker.window && !target_picker.window &&
                      destination_mode_mask()!=0u) {
                if(selected_operation==DOLLY_DEMO_CREATE_IMAGE) {
                    if(dolly_amiga_pick_file(v->window,&selected_target,1)) {
                        selected_target.image_format=selected_image_format();
                        refresh_main_selection();
                    }
                } else if(!dolly_picker_open(&target_picker,public_screen,1,
                                             &selected_target,
                                             medium_modes[1],DOLLY_MODE_BIT(medium_modes[1])))
                    dolly_picker_close(&target_picker);
                update_main_actions();
            } else if(cls==IDCMP_GADGETUP &&
                      (id==MAIN_OPERATION_ID || id==MAIN_SOURCE_MODE_ID ||
                       id==MAIN_TARGET_MODE_ID)) {
                if(operation_active() || source_picker.window || target_picker.window) {
                    unsigned column=id==MAIN_OPERATION_ID?0u:id==MAIN_SOURCE_MODE_ID?1u:2u;
                    if(v->selectors[column])
                        GT_SetGadgetAttrs(v->selectors[column],v->window,NULL,GTCY_Active,
                            (ULONG)(column?v->mode_active[column-1u]:selected_operation),TAG_DONE);
                } else if(!relayout && id==MAIN_OPERATION_ID && code<6u) {
                    selected_operation=code;
                    if(selected_operation==DOLLY_DEMO_CREATE_IMAGE)
                        selected_target.image_format=selected_image_format();
                    if(selected_source.kind!=DOLLY_ENDPOINT_NONE &&
                       dolly_mode_allowed(source_mode_mask(),dolly_endpoint_mode(&selected_source)))
                        medium_modes[0]=dolly_endpoint_mode(&selected_source);
                    if(selected_target.kind!=DOLLY_ENDPOINT_NONE &&
                       dolly_mode_allowed(destination_mode_mask(),dolly_endpoint_mode(&selected_target)))
                        medium_modes[1]=dolly_endpoint_mode(&selected_target);
                    relayout=1;
                } else if(!relayout && id!=MAIN_OPERATION_ID) {
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
            } else if(id==MAIN_START_ID && operation_active())toggle_pause();
            else if(id==MAIN_START_ID &&
                      operation.state!=DOLLY_OP_RUNNING &&
                      operation.state!=DOLLY_OP_PAUSED &&
                      !source_picker.window && !target_picker.window &&
                      !relayout && can_start_operation()) {
                quit_blocked=0;
                if(selected_operation==DOLLY_DEMO_INVENTORY) {
                    if(!dolly_inventory_view_open(&inventory_view,
                                                   public_screen,&selected_source))
                        puts("INVENTORY WARN: unable to open Inventory");
                } else if(selected_operation==DOLLY_DEMO_RESTORE) {
                    dolly_amiga_restore_preview(main_view.window,&selected_source,&selected_target);
                } else if(selected_operation==DOLLY_DEMO_COPY) {
                    dolly_amiga_copy_preview(main_view.window,&selected_source,&selected_target);
                } else {
                    uint64_t total=GRID_TOTAL;
                    scan_result=(unsigned char)(selected_operation==DOLLY_DEMO_SCAN);
                    image_result=(unsigned char)(selected_operation==DOLLY_DEMO_CREATE_IMAGE);
                    memset(cell_result,0,sizeof(cell_result));
                    if(scan_result) {
                        dolly_amiga_log_view_close();
                        dolly_operation_init(&operation);
                        total=dolly_surface_open(&surface,&selected_source,main_view.window,DOLLY_IO_SOURCE,1)?surface.total:0;
                    } else if(image_result) {
                        dolly_amiga_log_view_close();
                        dolly_operation_init(&operation);
                        total=dolly_image_open(&image,&selected_source,&selected_target,main_view.window)?
                              image.reader.total:0;
                    }
                    if(total)(void)dolly_operation_start(&operation,total);
                    update_numbers(&main_view);
                    if(sheep_view.window) {
                        draw_full(&sheep_view,0);
                        update_numbers(&sheep_view);
                    }
                    refresh_main_selection();
                }
            } else if(id==MAIN_STOP_ID && operation_active()) {
                ask_stop();
            } else if(id==MAIN_SHOW_ID && counter_available() &&
                      !show_sheep())
                puts("MAIN WARN: unable to show Sheep Counter");
        } else if(cls==IDCMP_CLOSEWINDOW || id==SHEEP_HIDE_ID)hide=1;
        if(main_window && (id==MAIN_SETTINGS_ID ||
           (cls==IDCMP_MENUPICK && code==IO_SETTINGS_ITEM)) &&
           io_settings_available())
            dolly_amiga_io_settings(public_screen,&selected_source);
        if(!main_window && id==SHEEP_PAUSE_ID && operation_active())toggle_pause();
        if(!main_window && id==SHEEP_LOG_ID && log_available() &&
           !dolly_amiga_log_view_open(public_screen,&surface,log_outcome()))
            puts("SURFACE SCAN: could not open log window");
        if(hide)break;
    }
    if(hide) {
        hide_sheep();
        return;
    }
    if(*quit)return;
    if(relayout) {
        if(!rebuild_gadgets(v,main_window)) {
            puts("WINDOW WARN: resize layout rejected");
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
int main(int argc,char **argv) {
    unsigned frames=0;
    int quit=0,rc=10;
    static struct TextAttr marker_attr={
        (STRPTR)"topaz.font",8,FS_NORMAL,FPF_ROMFONT
    };
    dolly_operation_init(&operation);
    puts("Dolly: entered main");
    /* Open the main UI idle. Starting a synthetic operation is explicit. */
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",36);
    if(!IntuitionBase)return 10;
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",36);
    GadToolsBase=OpenLibrary("gadtools.library",36);
    AslBase=OpenLibrary("asl.library",36); /* Optional file selector. */
    /* libnix startup already owns DOSBase; no extra OpenLibrary. */
    if(!GfxBase || !GadToolsBase)goto cleanup;
    marker_font=OpenFont(&marker_attr);
    if(!marker_font)goto cleanup;
    public_screen=LockPubScreen(NULL);
    if(!public_screen)goto cleanup;
    if((ULONG)public_screen->Width<640ul ||
       (ULONG)public_screen->Height<256ul)goto cleanup;
    screen_info=GetScreenDrawInfo(public_screen);
    if(!screen_info)goto cleanup;
    puts("Dolly: public screen ready");
    {
        ULONG top=(ULONG)public_screen->BarHeight+1ul;
        ULONG max_height=(ULONG)public_screen->Height-top;
        const struct TextFont *font=public_screen->RastPort.Font;
        ULONG height=150u+(ULONG)main_selector_extra(font)+
                     (ULONG)(selector_height(font)-MAIN_GADGET_HEIGHT);
        ULONG width=(ULONG)main_selector_width(&public_screen->RastPort)+32u;
        if(height>max_height)height=max_height;
        if(width<600u)width=600u;
        if(width>(ULONG)public_screen->Width-12u)width=(ULONG)public_screen->Width-12u;
        if(!open_view(&main_view,1,"Dolly",12,top,width,height))goto cleanup;
    }
    settings_menu=CreateMenus(settings_items,TAG_DONE);
    if(settings_menu &&
       (!LayoutMenus(settings_menu,main_view.visual,TAG_DONE) ||
        !SetMenuStrip(main_view.window,settings_menu))) {
        FreeMenus(settings_menu);
        settings_menu=NULL;
    }
    draw_full(&main_view,1);
    refresh_main_selection();
    {
        const char *error=dolly_amiga_prefs_load(argc,argv);
        if(error) {
            struct EasyStruct request={
                sizeof(struct EasyStruct),0,(STRPTR)"Dolly - Preferences",
                (STRPTR)"%s",(STRPTR)"OK"
            };
            ULONG args[]={(ULONG)error};
            EasyRequestArgs(main_view.window,&request,NULL,args);
        }
    }
    puts("Dolly: main window ready");
    /* Sheep Counter intentionally remains closed until explicitly shown. */
    while(!quit) {
        WaitTOF();
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
            selected_source=source_picker.result;
            source_picker.changed=0;
            if(!dolly_mode_allowed(destination_mode_mask(),
                                   dolly_endpoint_mode(&selected_target)))
                dolly_endpoint_clear(&selected_target);
            if(rebuild_gadgets(&main_view,1))draw_full(&main_view,1);
            refresh_main_selection();
        }
        if(target_picker.changed) {
            selected_target=target_picker.result;
            target_picker.changed=0;
            refresh_main_selection();
        }
        if(sheep_view.window)process_view(&sheep_view,0,&quit);
        dolly_inventory_view_pump(&inventory_view);
        dolly_amiga_log_view_pump();
        if(!quit && image.file && operation.state==DOLLY_OP_RUNNING) {
            unsigned old=progress_units(GRID_TOTAL);
            dolly_image_step(&image);
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
                printf("SURFACE SCAN: processed=%lu errors=%lu retries=%lu\n",
                       (unsigned long)surface.processed,(unsigned long)surface.errors,
                       (unsigned long)surface.retries);
                dolly_surface_close(&surface);
                refresh_main_selection();
            }
            progress_view_update(old,cell_changed);
        } else if(!quit && ++frames>=8u && operation.state==DOLLY_OP_RUNNING) {
            unsigned old;
            frames=0;
            if(operation.completed_blocks<GRID_TOTAL) {
                old=(unsigned)operation.completed_blocks;
                if(dolly_operation_progress(&operation,
                    operation.completed_blocks+1,
                    operation.errors,operation.retries)==DOLLY_OP_OK)
                    progress_view_update(old,0);
            } else {
                (void)dolly_operation_finish(&operation,1);
                refresh_main_selection();
            }
        }
        if(!quit)sync_action_labels();
    }
    printf("Session blocks=%lu/%lu state=%u visible=%u\n",
           (unsigned long)operation.completed_blocks,
           (unsigned long)operation.total_blocks,
           (unsigned)operation.state,
           (unsigned)operation.sheep_counter_visible);
    rc=0;
cleanup:
    dolly_image_cancel(&image);
    dolly_amiga_log_view_close();
    dolly_surface_close(&surface);
    dolly_surface_log_clear(&surface.log);
    dolly_inventory_view_close(&inventory_view);
    dolly_picker_close(&source_picker);
    dolly_picker_close(&target_picker);
    hide_sheep();
    if(settings_menu) {
        ClearMenuStrip(main_view.window);
        FreeMenus(settings_menu);
    }
    dolly_amiga_io_settings_clear();
    destroy_view(&main_view);
    if(screen_info && public_screen)
        FreeScreenDrawInfo(public_screen,screen_info);
    if(public_screen)UnlockPubScreen(NULL,public_screen);
    if(marker_font)CloseFont(marker_font);
     if(AslBase)CloseLibrary(AslBase);
    if(GadToolsBase)CloseLibrary(GadToolsBase);
    if(GfxBase)CloseLibrary((struct Library *)GfxBase);
    if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
