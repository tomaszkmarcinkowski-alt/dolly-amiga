/* M1.12: responsive, two-window GadTools OperationSession demo.
 * MC68000 / AmigaOS 2.04: system pens, live window borders, aspect margins,
 * recessed 24x8 grid and incremental (not full-screen) progress painting.
 * No actual disk/device operations. Sheep Hide never cancels operation.
 */
#include "dolly_operation_session.h"
#include "dolly_gui_minimum.h"
#include "dolly_endpoint.h"
#include "dolly_amiga_endpoint_picker.h"
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
#define MAIN_SHOW_ID 201u
#define MAIN_QUIT_ID 202u
#define SHEEP_HIDE_ID 203u
#define MAIN_COUNT_ID 204u
#define SHEEP_COUNT_ID 205u
#define SHEEP_ERRORS_ID 206u
#define SHEEP_PERCENT_ID 207u
#define MAIN_START_ID 208u
#define MAIN_DEVICE_ID 209u
#define MAIN_OPERATION_ID 210u
#define MAIN_STATUS_ID 211u
#define MAIN_SOURCE_ID 212u
#define MAIN_TARGET_ID 213u
#define MAIN_DESC_ID 214u
#define MAIN_SOURCE_TEXT_ID 215u
#define MAIN_TARGET_TEXT_ID 216u
#define MAIN_INVENTORY_ID 217u
#define MAIN_STOP_ID 218u
#define MAIN_PLACEHOLDER_ID 219u
#define DOLLY_DEMO_BUILD_LABEL "Dolly M1.42"
#define MAIN_CONTROLS_OFFSET 87
#define MAIN_PROGRESS_OFFSET 125
#define MAIN_STATUS_OFFSET 147
#define MAIN_GADGET_HEIGHT 15
#define MAIN_STATUS_BEVEL_HEIGHT 19

typedef struct DemoLayout {
    LONG left, right, top, bottom;
    LONG main_device_y, main_target_y, main_operation_y;
    LONG main_desc_y, main_status_y, main_count_y;
    LONG progress_x, progress_w, stop_x, quit_x, stop_quit_w;
    LONG pad_x, pad_y;
    LONG controls_y, bar_y, bar_right, percent_x, percent_w;
    LONG grid_x, grid_y, cell_w, cell_h;
    LONG grid_w, grid_h;
    LONG number_w, error_x, hide_x, hide_w;
} DemoLayout;
typedef struct DollyDemoView {
    struct Window *window;
    struct Gadget *gadgets;
    struct Gadget *show, *inventory, *placeholder, *stop, *quit;
    struct Gadget *start, *hide, *count, *errors, *percent;
    struct Gadget *device,*target_button,*source_text,*target_text;
    struct Gadget *description,*op_selector,*status;
    char source_label[72],target_label[72],desc_label[96];
    char status_label[64];
    char block_label[40];
    char percent_label[8];
    APTR visual;
    unsigned char gadgets_detached;
    DemoLayout l;
} DollyDemoView;

static struct Screen *public_screen;
static struct DrawInfo *screen_info;
static struct TextFont *marker_font;
static DollyOperationSession operation;
static DollyDemoView main_view, sheep_view;
/* SYNTHETIC device tree. This demo never opens physical media. */
static DollyEndpoint selected_source,selected_target;
static DollyEndpointPicker source_picker,target_picker;
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

static void text_copy(char *dst,size_t cap,const char *src) {
    size_t n=src?strlen(src):0u;
    if(n>=cap)n=cap-1u;
    if(n)memcpy(dst,src,n);
    dst[n]='\0';
}
static void format_endpoint(char *out,size_t cap,const DollyEndpoint *e) {
    char tmp[72];
    if(e->kind==DOLLY_ENDPOINT_NONE){
        text_copy(out,cap,"<not selected>");return;
    }
    if(e->kind==DOLLY_ENDPOINT_FILE){
        text_copy(out,cap,e->path);return;
    }
    if(e->kind==DOLLY_ENDPOINT_LOGICAL) {
        text_copy(out,cap,e->path[0] ? e->path :
                   "<logical device not resolved>");
        return;
    }
    if(e->path[0]) {
        text_copy(out,cap,e->path);
        return;
    }
    sprintf(tmp,"Device %u / unit %u",
            (unsigned)e->device,(unsigned)e->unit);
    if(e->kind==DOLLY_ENDPOINT_PARTITION)
        sprintf(tmp+strlen(tmp)," / partition %u",(unsigned)e->partition);
    else
        strcat(tmp," (whole)");
    text_copy(out,cap,tmp);
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
/* Image format is metadata of the selected output endpoint, not a
 * filename extension guess. Keep "(FORMAT)" visible when a long path
 * needs clipping, and do not modify the committed path itself.
 */
static const char *image_format_label(DollyImageFormat format) {
    switch(format) {
    case DOLLY_IMAGE_RAW:return "RAW";
    case DOLLY_IMAGE_HDF:return "HDF";
    case DOLLY_IMAGE_ADF:return "ADF";
    case DOLLY_IMAGE_IFF_DOLY:return "IFF DOLY";
    default:return "Unknown";
    }
}
static void format_destination_image(char *dst,size_t cap,
                                     const DollyEndpoint *endpoint,
                                     LONG max_pixels) {
    const char *name=endpoint->path;
    const char *fmt=image_format_label(endpoint->image_format);
    size_t name_len=strlen(name),fmt_len=strlen(fmt);
    size_t tail_len=fmt_len+3u; /* space, (, ) */
    struct RastPort *rp=main_view.window?main_view.window->RPort:NULL;
    if(!cap || cap<=tail_len)return;
    if(name_len>cap-1u-tail_len)name_len=cap-1u-tail_len;
    while(name_len) {
        memcpy(dst,name,name_len);
        dst[name_len]=' ';
        dst[name_len+1u]='(';
        memcpy(dst+name_len+2u,fmt,fmt_len);
        dst[name_len+2u+fmt_len]=')';
        dst[name_len+tail_len]='\0';
        if(!rp || (LONG)TextLength(rp,(STRPTR)dst,
                                   (ULONG)strlen(dst))<=max_pixels)
            return;
        --name_len;
    }
    /* Even when the filename is exceptionally long, preserve format. */
    dst[0]=' ';
    dst[1]='(';
    memcpy(dst+2,fmt,fmt_len);
    dst[2u+fmt_len]=')';
    dst[tail_len]='\0';
}
static void update_main_status(void) {
    int need=dolly_demo_needs_target((DollyDemoOperation)selected_operation);
    int can=dolly_demo_can_start((DollyDemoOperation)selected_operation,
                                &selected_source,&selected_target);
    text_copy(main_view.desc_label,sizeof(main_view.desc_label),
              demo_descriptions[selected_operation]);
    if(operation.state==DOLLY_OP_RUNNING)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Operation in progress...");
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
    else if(!can)
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Check source/destination identity or overlap.");
    else
        text_copy(main_view.status_label,sizeof(main_view.status_label),
                  "Ready. No disk I/O.");
    format_endpoint(main_view.source_label,sizeof(main_view.source_label),
                    &selected_source);
    format_endpoint(main_view.target_label,sizeof(main_view.target_label),
                    &selected_target);
    if(selected_operation==DOLLY_DEMO_CREATE_IMAGE &&
       selected_target.kind==DOLLY_ENDPOINT_FILE) {
        LONG pixel_width=main_view.window ?
                         main_view.l.right-main_view.l.left-120 : 9999;
        format_destination_image(main_view.target_label,
                                 sizeof(main_view.target_label),
                                 &selected_target,pixel_width);
    }
    if(main_view.window) {
        DemoLayout *d=&main_view.l;
        clip_main(main_view.source_label,d->right-d->left-120);
        if(!(selected_operation==DOLLY_DEMO_CREATE_IMAGE &&
             selected_target.kind==DOLLY_ENDPOINT_FILE))
            clip_main(main_view.target_label,d->right-d->left-120);
        clip_main(main_view.desc_label,d->right-d->left-88);
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
/* A future copy/recovery backend may opt into interruption only after
 * specifying safe cancellation boundaries. At present no such backend
 * has passed that review, so Stop cannot be enabled or invoked.
 */
static int operation_supports_safe_stop(void) {
    return 0;
}
static void update_labels(DollyDemoView *v) {
    char *p=v->block_label;
    char *limit=v->block_label+sizeof(v->block_label)-1;
    unsigned long current=(unsigned long)operation.completed_blocks;
    unsigned long total=(unsigned long)operation.total_blocks;
    unsigned long percent=total ? current*100ul/total : 0ul;
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
        /* Last required element defines the client height. */
        if(avail_w<320 ||
           avail_h<MAIN_STATUS_OFFSET-2+MAIN_STATUS_BEVEL_HEIGHT+2)
            return 0;
        /* Operation, Info, Source, Destination, action row.
         * A compact Progress/status pair replaces the old gap.
         */
        d.main_operation_y=d.top+3;
        d.main_desc_y=d.top+23;
        d.main_device_y=d.top+43;
        d.main_target_y=d.top+63;
        d.controls_y=d.top+MAIN_CONTROLS_OFFSET;
        d.main_count_y=d.top+MAIN_PROGRESS_OFFSET;
        d.main_status_y=d.top+MAIN_STATUS_OFFSET;
        /* Fixed compact percent well, Stop/Quit aligned to the right.
         * Keep enough room even at the 320px minimum client width.
         */
        d.progress_x=d.left+86;
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
        d.hide_w=76;
        d.hide_x=d.right-d.hide_w+1;
        d.controls_y=d.bottom-15;
        d.bar_y=d.controls_y-26;
        d.percent_w=46;
        d.percent_x=d.right-d.percent_w+1;
        d.bar_right=d.percent_x-7;
        grid_bottom=d.bar_y-7;
        d.grid_x=d.left+3;
        d.grid_y=d.top+19;
        d.grid_w=avail_w-8;
        d.grid_h=grid_bottom-d.grid_y;
        d.cell_w=d.grid_w/(LONG)GRID_COLS;
        d.cell_h=d.grid_h/(LONG)GRID_ROWS;
        if(d.cell_w<10 || d.cell_h<9)return 0;
        d.number_w=(d.hide_x-d.left-14)/2;
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
    ng.ng_Height=15;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)title;
    return CreateGadget(BUTTON_KIND,last,&ng,
                        GA_Disabled,(ULONG)(
                          (id==MAIN_INVENTORY_ID ||
                           id==MAIN_PLACEHOLDER_ID) ||
                          (id==MAIN_STOP_ID &&
                           (operation.state!=DOLLY_OP_RUNNING ||
                            !operation_supports_safe_stop())) ||
                          (id==MAIN_SHOW_ID &&
                           operation.state!=DOLLY_OP_RUNNING &&
                           operation.state!=DOLLY_OP_PAUSED) ||
                          (id==MAIN_START_ID &&
                           ((operation.state==DOLLY_OP_RUNNING ||
                             operation.state==DOLLY_OP_PAUSED) ||
                            !dolly_demo_can_start(
                             (DollyDemoOperation)selected_operation,
                             &selected_source,&selected_target))) ||
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
    ng.ng_Height=15;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(TEXT_KIND,last,&ng,
                        GTTX_Text,(ULONG)value,
                        GTTX_Border,border,
                        GTTX_Clipped,TRUE,TAG_DONE);
}
static struct Gadget *make_cycle(struct Gadget *last,DollyDemoView *v,
                                  UWORD id,LONG x,LONG y,LONG width,
                                  STRPTR *choices,UWORD active) {
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
    return CreateGadget(CYCLE_KIND,last,&ng,
                        GTCY_Labels,(ULONG)choices,
                        GTCY_Active,(ULONG)active,
                        GA_Disabled,(ULONG)(
                          operation.state==DOLLY_OP_RUNNING ||
                          operation.state==DOLLY_OP_PAUSED),
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
        v->show=NULL;v->inventory=NULL;v->placeholder=NULL;
        v->stop=NULL;v->quit=NULL;v->start=NULL;v->hide=NULL;
        v->count=NULL;v->errors=NULL;v->percent=NULL;
        v->device=NULL;v->target_button=NULL;
        v->source_text=NULL;v->target_text=NULL;
        v->description=NULL;v->op_selector=NULL;v->status=NULL;
    }
    last=CreateContext(&v->gadgets);
    if(!last)return 0;
    if(main_window) {
        LONG detail_x=d->left+120;
        LONG detail_w=d->right-detail_x+1;
        LONG operation_x=d->left+88;
        LONG three_width,three_gap=6;
        update_main_status();
        last=make_button(last,v,MAIN_SOURCE_ID,d->left,d->main_device_y,
                         114,"Source...");
        if(!last)return 0;
        v->device=last;
        last=make_text(last,v,MAIN_SOURCE_TEXT_ID,detail_x,d->main_device_y,
                       detail_w,v->source_label,TRUE);
        if(!last)return 0;
        v->source_text=last;
        last=make_button(last,v,MAIN_TARGET_ID,d->left,d->main_target_y,
                         114,"Destination...");
        if(!last)return 0;
        v->target_button=last;
        last=make_text(last,v,MAIN_TARGET_TEXT_ID,detail_x,d->main_target_y,
                       detail_w,v->target_label,TRUE);
        if(!last)return 0;
        v->target_text=last;
        last=make_cycle(last,v,MAIN_OPERATION_ID,operation_x,
                        d->main_operation_y,d->right-operation_x+1,
                        demo_operations,selected_operation);
        if(!last)return 0;
        v->op_selector=last;
        last=make_text(last,v,MAIN_DESC_ID,operation_x,d->main_desc_y,
                       d->right-operation_x+1,v->desc_label,FALSE);
        if(!last)return 0;
        v->description=last;
        three_width=(d->right-d->left+1-2*three_gap)/3;
        last=make_button(last,v,MAIN_SHOW_ID,d->left,d->controls_y,
                         three_width,"Sheep Counter");
        if(!last)return 0;
        v->show=last;
        last=make_button(last,v,MAIN_INVENTORY_ID,
                         d->left+three_width+three_gap,d->controls_y,
                         three_width,"");
        if(!last)return 0;
        v->inventory=last;
        last=make_button(last,v,MAIN_PLACEHOLDER_ID,
                         d->left+2*(three_width+three_gap),d->controls_y,
                         three_width,"");
        if(!last)return 0;
        v->placeholder=last;
        last=make_button(last,v,MAIN_START_ID,d->left,
                         d->controls_y+19,d->right-d->left+1,"Start");
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
        last=make_button(last,v,SHEEP_HIDE_ID,d->hide_x,d->controls_y,
                         d->hide_w,"Hide");
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
    v->show=NULL;v->inventory=NULL;v->placeholder=NULL;
    v->stop=NULL;v->quit=NULL;v->start=NULL;v->hide=NULL;
    v->count=NULL;v->errors=NULL;v->percent=NULL;
    v->device=NULL;v->target_button=NULL;
    v->source_text=NULL;v->target_text=NULL;
    v->description=NULL;v->op_selector=NULL;v->status=NULL;
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
        m.main_content_height+=3u;
        m.main_build_label_width=(ULONG)TextLength(rp,
            (STRPTR)DOLLY_DEMO_BUILD_LABEL,
            (ULONG)strlen(DOLLY_DEMO_BUILD_LABEL));
        {
            static const char heading[]="Operation:";
            static const char choice[]="Restore a file image to a destination.";
            ULONG a=(ULONG)TextLength(rp,(STRPTR)heading,
                                       (ULONG)strlen(heading));
            ULONG b=(ULONG)TextLength(rp,(STRPTR)choice,
                                       (ULONG)strlen(choice));
            m.heading_width=a>b+88ul?a:b+88ul;
        }
        m.show_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)"Sheep Counter",13);
        m.quit_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)"Quit",4);
    } else {
        m.heading_width=(ULONG)TextLength(rp,
                (STRPTR)"Surface Scan - Sheep Counter",28);
        m.hide_button_text_width=(ULONG)TextLength(rp,
                (STRPTR)"Hide",4);
    }
    minimum=dolly_gui_measure_minimum(
        main_window?DOLLY_GUI_WINDOW_MAIN:DOLLY_GUI_WINDOW_SHEEP,m);
    if(!minimum.valid || minimum.outer_width>(ULONG)w->Width ||
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
                 IDCMP_REFRESHWINDOW|IDCMP_NEWSIZE|IDCMP_SIZEVERIFY,
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
    if(v->errors)
        GT_SetGadgetAttrs(v->errors,v->window,NULL,
                          GTNM_Number,(ULONG)operation.errors,TAG_DONE);
}
static void draw_main_static(void) {
    DemoLayout *d=&main_view.l;
    struct TagItem tags[3];
    if(!main_view.window)return;
    label(main_view.window,d->left,d->main_operation_y+11,"Operation:");
    label(main_view.window,d->left,d->main_desc_y+11,"Info:");
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
                        LONG width,LONG height) {
    struct TextFont *saved=rp->Font;
    UBYTE mode=rp->DrawMode;
    LONG glyph_width;
    if(!marker_font)return;
    SetFont(rp,marker_font);
    glyph_width=(LONG)TextLength(rp,(STRPTR)"*",1);
    SetDrMd(rp,JAM1);
    SetAPen(rp,system_pen(TEXTPEN));
    Move(rp,x+(width-glyph_width)/2,
         y+(height-(LONG)marker_font->tf_YSize)/2+
           (LONG)marker_font->tf_Baseline);
    Text(rp,(STRPTR)"*",1);
    SetDrMd(rp,mode);
    if(saved)SetFont(rp,saved);
}
static void draw_cell(unsigned idx,int active) {
    DemoLayout *d=&sheep_view.l;
    struct RastPort *rp;
    LONG x,y,width,height;
    if(!sheep_view.window || idx>=GRID_TOTAL)return;
    rp=sheep_view.window->RPort;
    /* Divide the entire usable span, distributing leftover pixels
     * across cells instead of leaving a strip blank at the right.
     */
    x=d->grid_x+(LONG)((idx%GRID_COLS)*(unsigned long)d->grid_w/GRID_COLS);
    y=d->grid_y+(LONG)((idx/GRID_COLS)*(unsigned long)d->grid_h/GRID_ROWS);
    width=(LONG)(((idx%GRID_COLS+1u)*(unsigned long)d->grid_w/GRID_COLS)-
                 ((idx%GRID_COLS)*(unsigned long)d->grid_w/GRID_COLS))-2;
    height=(LONG)(((idx/GRID_COLS+1u)*(unsigned long)d->grid_h/GRID_ROWS)-
                  ((idx/GRID_COLS)*(unsigned long)d->grid_h/GRID_ROWS))-2;
    fill(rp,system_pen(SHADOWPEN),x,y,x+width,y+height);
    fill(rp,system_pen(idx<operation.completed_blocks?
                       FILLPEN:BACKGROUNDPEN),
         x+1,y+1,x+width-1,y+height-1);
    if(active)draw_marker(rp,x+1,y+1,width-1,height-1);
}
/* The bezel is system-supplied and recessed just like the grid's frame.
 * The fill has no drawn outline: background / FILLPEN only in the well.
 */
static void draw_progress_bar(void) {
    DemoLayout *d=&sheep_view.l;
    struct RastPort *rp=sheep_view.window->RPort;
    struct TagItem tags[3];
    LONG interior_w=d->bar_right-d->left-3;
    LONG filled=(LONG)(operation.completed_blocks*
                       (unsigned long)interior_w/GRID_TOTAL);
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
    if(filled>0)
        fill(rp,system_pen(FILLPEN),
             d->left+2,d->bar_y+2,d->left+1+filled,d->bar_y+7);
}
static void draw_sheep_static(void) {
    DemoLayout *d=&sheep_view.l;
    struct RastPort *rp;
    struct TagItem tags[3];
    unsigned i;
    if(!sheep_view.window)return;
    rp=sheep_view.window->RPort;
    label(sheep_view.window,d->left,d->top+10,
          "Surface Scan - Sheep Counter");
    tags[0].ti_Tag=GT_VisualInfo;
    tags[0].ti_Data=(ULONG)sheep_view.visual;
    tags[1].ti_Tag=GTBB_Recessed;
    tags[1].ti_Data=TRUE;
    tags[2].ti_Tag=TAG_DONE;
    tags[2].ti_Data=0;
    DrawBevelBoxA(rp,d->grid_x-2,d->grid_y-2,
                  d->grid_w+4,d->grid_h+4,tags);
    for(i=0;i<GRID_TOTAL;++i)
        draw_cell(i,i==operation.completed_blocks);
    draw_progress_bar();
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
    if(operation.state!=DOLLY_OP_RUNNING &&
       operation.state!=DOLLY_OP_PAUSED)return 0;
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
static void update_main_actions(void) {
    int active=(operation.state==DOLLY_OP_RUNNING ||
                operation.state==DOLLY_OP_PAUSED);
    if(!main_view.window)return;
    if(main_view.inventory)
        GT_SetGadgetAttrs(main_view.inventory,main_view.window,NULL,
                          GA_Disabled,TRUE,TAG_DONE);
    if(main_view.placeholder)
        GT_SetGadgetAttrs(main_view.placeholder,main_view.window,NULL,
                          GA_Disabled,TRUE,TAG_DONE);
    if(main_view.stop)
        GT_SetGadgetAttrs(main_view.stop,main_view.window,NULL,
                          GA_Disabled,(ULONG)(operation.state!=DOLLY_OP_RUNNING ||
                                  !operation_supports_safe_stop()),TAG_DONE);
    if(main_view.show)
        GT_SetGadgetAttrs(main_view.show,main_view.window,NULL,
                          GA_Disabled,(ULONG)!active,TAG_DONE);
    if(main_view.start)
        GT_SetGadgetAttrs(main_view.start,main_view.window,NULL,
                          GA_Disabled,(ULONG)(active ||
                          source_picker.window || target_picker.window ||
                          !dolly_demo_can_start(
                            (DollyDemoOperation)selected_operation,
                            &selected_source,&selected_target)),TAG_DONE);
    if(main_view.device)
        GT_SetGadgetAttrs(main_view.device,main_view.window,NULL,
                          GA_Disabled,(ULONG)active,TAG_DONE);
    if(main_view.target_button)
        GT_SetGadgetAttrs(main_view.target_button,main_view.window,NULL,
                          GA_Disabled,(ULONG)(active ||
                          !destination_mode_mask()),TAG_DONE);
    if(main_view.op_selector)
        GT_SetGadgetAttrs(main_view.op_selector,main_view.window,NULL,
                          GA_Disabled,(ULONG)(active ||
                              source_picker.window ||
                              target_picker.window),TAG_DONE);
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
static void progress_view_update(unsigned old) {
    DemoLayout *d=&sheep_view.l;
    LONG bar_w,old_pixels,new_pixels;
    update_numbers(&main_view);
    if(!sheep_view.window)return;
    if(old<GRID_TOTAL)draw_cell(old,0);
    if(operation.completed_blocks<GRID_TOTAL)
        draw_cell((unsigned)operation.completed_blocks,1);
    bar_w=d->bar_right-d->left-3;
    old_pixels=(LONG)(old*(unsigned long)bar_w/GRID_TOTAL);
    new_pixels=(LONG)(operation.completed_blocks*
                      (unsigned long)bar_w/GRID_TOTAL);
    if(new_pixels>old_pixels)
        fill(sheep_view.window->RPort,system_pen(FILLPEN),
             d->left+2+old_pixels,d->bar_y+2,
             d->left+1+new_pixels,d->bar_y+7);
    update_numbers(&sheep_view);
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
        if(main_window) {
            if(cls==IDCMP_CLOSEWINDOW || id==MAIN_QUIT_ID)*quit=1;
            else if(id==MAIN_SOURCE_ID) {
                dolly_picker_close(&target_picker);
                if(!dolly_picker_open(&source_picker,public_screen,0,
                                      &selected_source,
                                      selected_operation==DOLLY_DEMO_RESTORE ?
                                      DOLLY_MODE_FILE:DOLLY_MODE_PHYSICAL,
                                      source_mode_mask()))
                    dolly_picker_close(&source_picker);
                update_main_actions();
            } else if(id==MAIN_TARGET_ID &&
                      destination_mode_mask()!=0u) {
                dolly_picker_close(&source_picker);
                if(!dolly_picker_open(&target_picker,public_screen,1,
                                      &selected_target,
                                      selected_operation==DOLLY_DEMO_CREATE_IMAGE ||
                                      selected_source.kind==DOLLY_ENDPOINT_FILE ?
                                      DOLLY_MODE_FILE:DOLLY_MODE_PHYSICAL,
                                      destination_mode_mask()))
                    dolly_picker_close(&target_picker);
                update_main_actions();
            } else if(id==MAIN_OPERATION_ID && code<6u &&
                      !source_picker.window && !target_picker.window) {
                selected_operation=code;
                refresh_main_selection();
            } else if(id==MAIN_START_ID &&
                      !source_picker.window && !target_picker.window &&
                      dolly_demo_can_start(
                        (DollyDemoOperation)selected_operation,
                        &selected_source,&selected_target)) {
                if(dolly_operation_start(&operation,GRID_TOTAL)==DOLLY_OP_OK) {
                    update_numbers(&main_view);
                    if(sheep_view.window) {
                        draw_full(&sheep_view,0);
                        update_numbers(&sheep_view);
                    }
                    refresh_main_selection();
                }
            } else if(id==MAIN_SHOW_ID &&
                      (operation.state==DOLLY_OP_RUNNING ||
                       operation.state==DOLLY_OP_PAUSED) &&
                      !show_sheep())
                puts("MAIN WARN: unable to show Sheep Counter");
        } else if(cls==IDCMP_CLOSEWINDOW || id==SHEEP_HIDE_ID)hide=1;
        GT_ReplyIMsg(msg);
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
}
int main(void) {
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
    if(!open_view(&main_view,1,"Dolly",
                  12,(ULONG)public_screen->BarHeight+1ul,600,190))
        goto cleanup;
    draw_full(&main_view,1);
    refresh_main_selection();
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
            refresh_main_selection();
        }
        if(target_picker.changed) {
            selected_target=target_picker.result;
            target_picker.changed=0;
            refresh_main_selection();
        }
        if(sheep_view.window)process_view(&sheep_view,0,&quit);
        if(++frames>=8u && operation.state==DOLLY_OP_RUNNING) {
            unsigned old;
            frames=0;
            if(operation.completed_blocks<GRID_TOTAL) {
                old=(unsigned)operation.completed_blocks;
                if(dolly_operation_progress(&operation,
                    operation.completed_blocks+1,
                    operation.errors,operation.retries)==DOLLY_OP_OK)
                    progress_view_update(old);
            } else {
                (void)dolly_operation_finish(&operation,1);
                refresh_main_selection();
            }
        }
    }
    printf("Session blocks=%lu/192 state=%u visible=%u\n",
           (unsigned long)operation.completed_blocks,
           (unsigned)operation.state,
           (unsigned)operation.sheep_counter_visible);
    rc=0;
cleanup:
    dolly_picker_close(&source_picker);
    dolly_picker_close(&target_picker);
    hide_sheep();
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
