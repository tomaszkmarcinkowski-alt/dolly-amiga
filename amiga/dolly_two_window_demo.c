/* M1.12: responsive, two-window GadTools OperationSession demo.
 * MC68000 / AmigaOS 2.04: system pens, live window borders, aspect margins,
 * recessed 24x8 grid and incremental (not full-screen) progress painting.
 * No actual disk/device operations. Sheep Hide never cancels operation.
 */
#include "dolly_operation_session.h"
#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
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
#define MAIN_MIN_W 300u
#define MAIN_MIN_H 112u
#define SHEEP_MIN_W 420u
#define SHEEP_MIN_H 205u

typedef struct DemoLayout {
    LONG left, right, top, bottom;
    LONG pad_x, pad_y;
    LONG controls_y, bar_y, bar_right, percent_x, percent_w;
    LONG grid_x, grid_y, cell_w, cell_h;
    LONG grid_w, grid_h;
    LONG number_w, error_x, hide_x, hide_w;
} DemoLayout;
typedef struct DollyDemoView {
    struct Window *window;
    struct Gadget *gadgets;
    struct Gadget *show, *quit, *hide, *count, *errors, *percent;
    char block_label[40];
    char percent_label[8];
    APTR visual;
    DemoLayout l;
} DollyDemoView;

static struct Screen *public_screen;
static struct DrawInfo *screen_info;
static struct TextFont *marker_font;
static DollyOperationSession operation;
static DollyDemoView main_view, sheep_view;

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
static void update_labels(DollyDemoView *v) {
    char *p=v->block_label;
    char *limit=v->block_label+sizeof(v->block_label)-1;
    unsigned long current=(unsigned long)operation.completed_blocks;
    unsigned long total=(unsigned long)operation.total_blocks;
    unsigned long percent=total ? current*100ul/total : 0ul;
    append_u32(&p,limit,current);
    append_char(&p,limit,' ');
    append_char(&p,limit,'/');
    append_char(&p,limit,' ');
    append_u32(&p,limit,total);
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
        if(avail_w<250 || avail_h<70)return 0;
        d.controls_y=d.top+19;
        d.number_w=avail_w/3;
        if(d.number_w>124)d.number_w=124;
    } else {
        if(avail_w<380 || avail_h<160)return 0;
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
    return CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
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
                        GTTX_Border,border,TAG_DONE);
}
static int rebuild_gadgets(DollyDemoView *v,int main_window) {
    struct Gadget *last;
    DemoLayout *d=&v->l;
    LONG button_w;
    if(!calculate_layout(v,main_window))return 0;
    update_labels(v);
    if(v->gadgets) {
        RemoveGList(v->window,v->gadgets,-1);
        FreeGadgets(v->gadgets);
        v->gadgets=NULL;
        v->show=NULL;v->quit=NULL;v->hide=NULL;
        v->count=NULL;v->errors=NULL;v->percent=NULL;
    }
    last=CreateContext(&v->gadgets);
    if(!last)return 0;
    if(main_window) {
        button_w=(d->right-d->left-6)/2;
        last=make_button(last,v,MAIN_SHOW_ID,d->left,d->controls_y,
                         button_w,"Sheep Counter");
        if(!last)return 0;
        v->show=last;
        last=make_button(last,v,MAIN_QUIT_ID,d->left+button_w+6,
                         d->controls_y,button_w,"Quit demo");
        if(!last)return 0;
        v->quit=last;
        last=make_text(last,v,MAIN_COUNT_ID,d->left,d->bottom-15,
                       d->number_w,v->block_label,TRUE);
        if(!last)return 0;
        v->count=last;
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
    RefreshGList(v->gadgets,v->window,NULL,-1);
    GT_RefreshWindow(v->window,NULL);
    return 1;
}
static void destroy_view(DollyDemoView *v) {
    if(v->window) {
        CloseWindow(v->window);
        v->window=NULL;
    }
    if(v->gadgets) {FreeGadgets(v->gadgets);v->gadgets=NULL;}
    if(v->visual) {FreeVisualInfo(v->visual);v->visual=NULL;}
    v->show=NULL;v->quit=NULL;v->hide=NULL;
    v->count=NULL;v->errors=NULL;v->percent=NULL;
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
        WA_MinWidth,main_window?MAIN_MIN_W:SHEEP_MIN_W,
        WA_MinHeight,main_window?MAIN_MIN_H:SHEEP_MIN_H,
        WA_MaxWidth,(ULONG)public_screen->Width,
        WA_MaxHeight,(ULONG)public_screen->Height-
                     ((ULONG)public_screen->BarHeight+1ul),
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,
        WA_CloseGadget,TRUE,WA_SizeGadget,TRUE,
        WA_Activate,TRUE,WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|
                 IDCMP_REFRESHWINDOW|IDCMP_NEWSIZE,
        TAG_DONE);
    if(!v->window)return 0;
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
    if(!main_view.window)return;
    /* Labels are not painted over system GadTools images. */
    label(main_view.window,d->left,d->top+10,"Dolly main window (synthetic)");
    label(main_view.window,d->left,d->controls_y+31,
          "Sheep Counter is an auxiliary window.");
    label(main_view.window,d->left,d->bottom-20,"Blocks:");
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
          "Surface scan (synthetic, read only)");
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
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_NEWSIZE)relayout=1;
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(v->window);
            GT_EndRefresh(v->window,TRUE);
            redraw=1;
        }
        if(main_window) {
            if(cls==IDCMP_CLOSEWINDOW || id==MAIN_QUIT_ID)*quit=1;
            else if(id==MAIN_SHOW_ID && !show_sheep())
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
            return;
        }
        redraw=1;
    }
    if(redraw)draw_full(v,main_window);
}
int main(void) {
    unsigned frames=0;
    int quit=0,rc=10;
    static struct TextAttr marker_attr={
        (STRPTR)"topaz.font",8,FS_NORMAL,FPF_ROMFONT
    };
    dolly_operation_init(&operation);
    if(dolly_operation_start(&operation,GRID_TOTAL)!=DOLLY_OP_OK)return 10;
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",36);
    if(!IntuitionBase)return 10;
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",36);
    GadToolsBase=OpenLibrary("gadtools.library",36);
    if(!GfxBase || !GadToolsBase)goto cleanup;
    marker_font=OpenFont(&marker_attr);
    if(!marker_font)goto cleanup;
    public_screen=LockPubScreen(NULL);
    if(!public_screen)goto cleanup;
    if((ULONG)public_screen->Width<640ul ||
       (ULONG)public_screen->Height<256ul)goto cleanup;
    screen_info=GetScreenDrawInfo(public_screen);
    if(!screen_info)goto cleanup;
    if(!open_view(&main_view,1,"Dolly - Main window (demo)",
                  14,(ULONG)public_screen->BarHeight+14ul,390,190))
        goto cleanup;
    draw_full(&main_view,1);
    if(!show_sheep())puts("MAIN WARN: unable to show Sheep Counter");
    while(!quit) {
        WaitTOF();
        process_view(&main_view,1,&quit);
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
            }
        }
    }
    printf("SESSION blocks=%lu/192 state=%u visible=%u\n",
           (unsigned long)operation.completed_blocks,
           (unsigned)operation.state,
           (unsigned)operation.sheep_counter_visible);
    rc=0;
cleanup:
    hide_sheep();
    destroy_view(&main_view);
    if(screen_info && public_screen)
        FreeScreenDrawInfo(public_screen,screen_info);
    if(public_screen)UnlockPubScreen(NULL,public_screen);
    if(marker_font)CloseFont(marker_font);
    if(GadToolsBase)CloseLibrary(GadToolsBase);
    if(GfxBase)CloseLibrary((struct Library *)GfxBase);
    if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
