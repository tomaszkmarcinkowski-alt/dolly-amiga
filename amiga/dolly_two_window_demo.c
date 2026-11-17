/* M1.11: two Intuition/GadTools windows, one shared OperationSession.
 * Classic AmigaOS 2.04 / MC68000 / 512 KiB Chip RAM. Synthetic only.
 * Main remains open while the auxiliary Sheep Counter is shown or hidden.
 * All numeric displays use GadTools NUMBER_KIND, not repeated Text/sprintf.
 * The 24x8 grid is drawn incrementally without a backing bitmap.
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

typedef struct DollyDemoView {
    struct Window *window;
    struct Gadget *gadgets;
    struct Gadget *show, *quit, *hide, *count, *errors;
    APTR visual;
} DollyDemoView;

static struct Screen *public_screen;
static struct DrawInfo *screen_info;
static struct TextFont *marker_font;
static DollyOperationSession operation;
static DollyDemoView main_view, sheep_view;

static void fill(struct RastPort *rp, UWORD pen,
                 LONG x1, LONG y1, LONG x2, LONG y2) {
    if (x2<x1 || y2<y1) return;
    SetAPen(rp,pen);
    RectFill(rp,x1,y1,x2,y2);
}
static void label(struct Window *w,LONG x,LONG y,const char *s) {
    struct RastPort *rp=w->RPort;
    SetAPen(rp,screen_info->dri_Pens[TEXTPEN]);
    Move(rp,x,y);
    Text(rp,(STRPTR)s,(ULONG)strlen(s));
}
static struct Gadget *button(struct Gadget *last,DollyDemoView *v,
                             UWORD id,WORD x,WORD y,WORD width,
                             const char *title) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=public_screen->Font;
    ng.ng_LeftEdge=x;
    ng.ng_TopEdge=y;
    ng.ng_Width=width;
    ng.ng_Height=17;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)title;
    return CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
}
static struct Gadget *number(struct Gadget *last,DollyDemoView *v,
                             UWORD id,WORD x,WORD y,WORD width,
                             ULONG value) {
    struct NewGadget ng;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=public_screen->Font;
    ng.ng_LeftEdge=x;
    ng.ng_TopEdge=y;
    ng.ng_Width=width;
    ng.ng_Height=16;
    ng.ng_GadgetID=id;
    ng.ng_GadgetText=(STRPTR)"";
    return CreateGadget(NUMBER_KIND,last,&ng,
                        GTNM_Number,value,
                        GTNM_Border,TRUE,TAG_DONE);
}
static int create_gadgets(DollyDemoView *v,int main_window) {
    struct Gadget *last;
    v->visual=GetVisualInfo(public_screen,TAG_DONE);
    if(!v->visual)return 0;
    last=CreateContext(&v->gadgets);
    if(!last)return 0;
    if(main_window) {
        last=button(last,v,MAIN_SHOW_ID,16,28,118,"Sheep Counter");
        if(!last)return 0;
        v->show=last;
        last=button(last,v,MAIN_QUIT_ID,150,28,118,"Quit demo");
        if(!last)return 0;
        v->quit=last;
        last=number(last,v,MAIN_COUNT_ID,20,111,110,
                    (ULONG)operation.completed_blocks);
        if(!last)return 0;
        v->count=last;
    } else {
        last=number(last,v,SHEEP_COUNT_ID,20,156,100,
                    (ULONG)operation.completed_blocks);
        if(!last)return 0;
        v->count=last;
        last=number(last,v,SHEEP_ERRORS_ID,153,156,100,
                    (ULONG)operation.errors);
        if(!last)return 0;
        v->errors=last;
        last=button(last,v,SHEEP_HIDE_ID,278,155,88,"Hide");
        if(!last)return 0;
        v->hide=last;
    }
    return 1;
}
static void destroy_view(DollyDemoView *v) {
    if(v->window) {
        CloseWindow(v->window);
        v->window=NULL;
    }
    if(v->gadgets) {
        FreeGadgets(v->gadgets);
        v->gadgets=NULL;
    }
    if(v->visual) {
        FreeVisualInfo(v->visual);
        v->visual=NULL;
    }
    v->show=NULL; v->quit=NULL; v->hide=NULL;
    v->count=NULL; v->errors=NULL;
}
static int open_view(DollyDemoView *v,const char *title,
                     ULONG left,ULONG top,ULONG width,ULONG height) {
    v->window=OpenWindowTags(NULL,
        WA_PubScreen,(ULONG)public_screen,
        WA_Title,(ULONG)title,
        WA_Left,left,WA_Top,top,WA_Width,width,WA_Height,height,
        WA_Gadgets,(ULONG)v->gadgets,
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,
        WA_CloseGadget,TRUE,WA_Activate,TRUE,
        WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_REFRESHWINDOW,
        TAG_DONE);
    if(!v->window)return 0;
    GT_RefreshWindow(v->window,NULL);
    return 1;
}
static void update_numbers(DollyDemoView *v) {
    if(!v->window || !v->count)return;
    /* The synthetic demo has only 192 blocks: always within ULONG range.
     * Real 64-bit operations need paging/units, not silent truncation.
     */
    GT_SetGadgetAttrs(v->count,v->window,NULL,
                      GTNM_Number,(ULONG)operation.completed_blocks,
                      TAG_DONE);
    if(v->errors)
        GT_SetGadgetAttrs(v->errors,v->window,NULL,
                          GTNM_Number,(ULONG)operation.errors,TAG_DONE);
}
static void draw_main_labels(void) {
    if(!main_view.window)return;
    label(main_view.window,20,82,"Main window remains open.");
    label(main_view.window,20,96,"Hide keeps the scan running.");
    label(main_view.window,20,108,"Blocks:");
    label(main_view.window,138,124,"/ 192 (synthetic)");
}
static void draw_marker(struct RastPort *rp,LONG x,LONG y) {
    struct TextFont *saved=rp->Font;
    UBYTE old_mode=rp->DrawMode;
    LONG char_width;
    if(!marker_font)return;
    SetFont(rp,marker_font);
    SetDrMd(rp,JAM1);
    SetAPen(rp,screen_info->dri_Pens[TEXTPEN]);
    char_width=(LONG)TextLength(rp,(STRPTR)"*",1);
    Move(rp,x+(12-char_width)/2,
         y+(10-(LONG)marker_font->tf_YSize)/2+
           (LONG)marker_font->tf_Baseline);
    Text(rp,(STRPTR)"*",1);
    SetDrMd(rp,old_mode);
    if(saved)SetFont(rp,saved);
}
static void draw_cell(unsigned idx,int is_active) {
    struct RastPort *rp;
    LONG x,y;
    if(!sheep_view.window || idx>=GRID_TOTAL)return;
    rp=sheep_view.window->RPort;
    x=(LONG)sheep_view.window->BorderLeft+16+(LONG)(idx%GRID_COLS)*14;
    y=(LONG)sheep_view.window->BorderTop+18+(LONG)(idx/GRID_COLS)*12;
    fill(rp,screen_info->dri_Pens[SHADOWPEN],x,y,x+11,y+9);
    fill(rp,screen_info->dri_Pens[
         idx<operation.completed_blocks?FILLPEN:BACKGROUNDPEN],
         x+1,y+1,x+10,y+8);
    if(is_active)draw_marker(rp,x,y);
}
static void draw_grid_initial(void) {
    unsigned idx;
    if(!sheep_view.window)return;
    for(idx=0;idx<GRID_TOTAL;++idx)
        draw_cell(idx,idx==operation.completed_blocks);
}
static void draw_sheep_labels(void) {
    if(!sheep_view.window)return;
    label(sheep_view.window,18,149,"Blocks:");
    label(sheep_view.window,153,149,"Errors:");
}
static int show_sheep(void) {
    ULONG top=(ULONG)public_screen->BarHeight+1ul;
    if(sheep_view.window)return 1;
    if(!create_gadgets(&sheep_view,0)) {
        destroy_view(&sheep_view);
        return 0;
    }
    if(!open_view(&sheep_view,"Dolly - Sheep Counter",
                  100,top,410,222)) {
        destroy_view(&sheep_view);
        return 0;
    }
    dolly_operation_show_sheep_counter(&operation);
    draw_grid_initial();
    draw_sheep_labels();
    update_numbers(&sheep_view);
    return 1;
}
static void hide_sheep(void) {
    destroy_view(&sheep_view);
    dolly_operation_hide_sheep_counter(&operation);
}
static void advance_view(unsigned previous) {
    update_numbers(&main_view);
    if(!sheep_view.window)return;
    if(previous<GRID_TOTAL)draw_cell(previous,0);
    if(operation.completed_blocks<GRID_TOTAL)
        draw_cell((unsigned)operation.completed_blocks,1);
    update_numbers(&sheep_view);
}
static void process_view(DollyDemoView *v,int main_window,int *quit) {
    struct IntuiMessage *msg;
    if(!v->window)return;
    while(v->window && (msg=GT_GetIMsg(v->window->UserPort))!=NULL) {
        ULONG cls=msg->Class;
        UWORD id=0;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(v->window);
            GT_EndRefresh(v->window,TRUE);
            if(main_window)draw_main_labels();
            else {
                draw_grid_initial();
                draw_sheep_labels();
            }
        }
        GT_ReplyIMsg(msg);
        if(main_window) {
            if(cls==IDCMP_CLOSEWINDOW || id==MAIN_QUIT_ID)
                *quit=1;
            else if(id==MAIN_SHOW_ID) {
                if(!show_sheep())puts("MAIN WARN: Sheep Counter unavailable");
            }
        } else if(cls==IDCMP_CLOSEWINDOW || id==SHEEP_HIDE_ID) {
            hide_sheep();
            return;
        }
    }
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
    if(!create_gadgets(&main_view,1))goto cleanup;
    if(!open_view(&main_view,"Dolly - Main window (demo)",
                  14,(ULONG)public_screen->BarHeight+14ul,390,190))
        goto cleanup;
    draw_main_labels();
    if(!show_sheep())puts("MAIN WARN: Sheep Counter unavailable");
    while(!quit) {
        WaitTOF();
        process_view(&main_view,1,&quit);
        if(sheep_view.window)process_view(&sheep_view,0,&quit);
        if(++frames>=8u && operation.state==DOLLY_OP_RUNNING) {
            unsigned previous;
            frames=0;
            if(operation.completed_blocks<GRID_TOTAL) {
                previous=(unsigned)operation.completed_blocks;
                if(dolly_operation_progress(&operation,
                     operation.completed_blocks+1,operation.errors,
                     operation.retries)==DOLLY_OP_OK)
                    advance_view(previous);
            } else {
                (void)dolly_operation_finish(&operation,1);
            }
        }
    }
    printf("SESSION finished, %lu processed, visible=%u\n",
           (unsigned long)operation.completed_blocks,
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
