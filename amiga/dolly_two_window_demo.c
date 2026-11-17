/* M1.10: two-window, shared-session concept on AmigaOS 2.04.
 * Main window always remains open while Sheep Counter is visible.
 * Hiding Sheep Counter NEVER stops the synthetic operation.
 * No physical device or file I/O.
 */
#include "dolly_operation_session.h"
#include <exec/types.h>
#include <exec/libraries.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <libraries/gadtools.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <stdio.h>
#include <string.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase;

#define GRID_TOTAL 192u
#define MAIN_SHOW_ID 201u
#define MAIN_QUIT_ID 202u
#define SHEEP_HIDE_ID 203u

typedef struct DollyDemoView {
    struct Window *window;
    struct Gadget *list;
    struct Gadget *show;
    struct Gadget *quit;
    struct Gadget *hide;
    APTR visual;
} DollyDemoView;
static struct Screen *public_screen;
static struct DrawInfo *draw_info;
static DollyOperationSession operation;
static DollyDemoView main_view, sheep_view;

static int add_buttons(DollyDemoView *v, int main_window) {
    struct NewGadget ng;
    struct Gadget *last;
    memset(&ng,0,sizeof(ng));
    v->visual=GetVisualInfo(public_screen,TAG_DONE);
    if(!v->visual)return 0;
    last=CreateContext(&v->list);
    if(!last)return 0;
    ng.ng_VisualInfo=v->visual;
    ng.ng_TextAttr=public_screen->Font;
    ng.ng_TopEdge=45;
    ng.ng_Height=17;
    ng.ng_Width=118;
    ng.ng_Flags=0;
    if(main_window) {
        ng.ng_LeftEdge=16;
        ng.ng_GadgetID=MAIN_SHOW_ID;
        ng.ng_GadgetText=(STRPTR)"Sheep Counter";
        last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
        if(!last)return 0;
        v->show=last;
        ng.ng_LeftEdge=150;
        ng.ng_GadgetID=MAIN_QUIT_ID;
        ng.ng_GadgetText=(STRPTR)"Quit demo";
        last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
        if(!last)return 0;
        v->quit=last;
    } else {
        ng.ng_LeftEdge=16;
        ng.ng_TopEdge=173;
        ng.ng_GadgetID=SHEEP_HIDE_ID;
        ng.ng_GadgetText=(STRPTR)"Hide";
        last=CreateGadget(BUTTON_KIND,last,&ng,TAG_DONE);
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
    if(v->list) {FreeGadgets(v->list);v->list=NULL;}
    if(v->visual) {FreeVisualInfo(v->visual);v->visual=NULL;}
    v->show=NULL;v->quit=NULL;v->hide=NULL;
}
static struct Window *open_window(DollyDemoView *v, const char *title,
                                  ULONG left,ULONG top,
                                  ULONG width,ULONG height) {
    struct Window *w=OpenWindowTags(NULL,
        WA_PubScreen,(ULONG)public_screen,
        WA_Title,(ULONG)title,
        WA_Left,left,WA_Top,top,
        WA_Width,width,WA_Height,height,
        WA_Gadgets,(ULONG)v->list,
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,
        WA_CloseGadget,TRUE,WA_Activate,TRUE,
        WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|IDCMP_REFRESHWINDOW,
        TAG_DONE);
    if(w) {
        v->window=w;
        GT_RefreshWindow(w,NULL);
    }
    return w;
}
static void draw_text(struct Window *w,LONG x,LONG y,const char *s) {
    struct RastPort *rp=w->RPort;
    SetAPen(rp,draw_info->dri_Pens[TEXTPEN]);
    Move(rp,x,y);
    Text(rp,(STRPTR)s,(ULONG)strlen(s));
}
static void draw_main(void) {
    char line[100];
    if(!main_view.window)return;
    sprintf(line,"Core operation: %lu / %lu blocks",
            (unsigned long)operation.completed_blocks,
            (unsigned long)operation.total_blocks);
    draw_text(main_view.window,20,98,
              "Main window remains open while Sheep Counter is shown.");
    draw_text(main_view.window,20,117,
              "Hide closes only the secondary progress view.");
    /* Clear fixed status line before displaying updated counter. */
    SetAPen(main_view.window->RPort,draw_info->dri_Pens[BACKGROUNDPEN]);
    RectFill(main_view.window->RPort,18,135,370,149);
    draw_text(main_view.window,20,146,line);
}
static void draw_sheep(void) {
    char line[80];
    ULONG done,cols=24,rows=8,col,row;
    LONG x0,y0,dx=12,dy=11;
    struct RastPort *rp;
    if(!sheep_view.window)return;
    rp=sheep_view.window->RPort;
    done=(ULONG)operation.completed_blocks;
    x0=sheep_view.window->BorderLeft+16;
    y0=sheep_view.window->BorderTop+20;
    for(row=0;row<rows;++row)for(col=0;col<cols;++col) {
        ULONG idx=row*cols+col;
        LONG x=x0+(LONG)col*dx,y=y0+(LONG)row*dy;
        SetAPen(rp,draw_info->dri_Pens[idx<done?FILLPEN:BACKGROUNDPEN]);
        RectFill(rp,x,y,x+9,y+8);
    }
    /* Cursor uses the minimal system-compatible single character here.
     * The proven resizable Sheep Counter remains in its own executable.
     */
    if(done<GRID_TOTAL) {
        LONG x=x0+(LONG)(done%cols)*dx;
        LONG y=y0+(LONG)(done/cols)*dy;
        draw_text(sheep_view.window,x+1,y+8,"*");
    }
    SetAPen(rp,draw_info->dri_Pens[BACKGROUNDPEN]);
    RectFill(rp,16,135,365,153);
    sprintf(line,"Blocks: %lu / %u  |  Errors: %lu",
            (unsigned long)done,GRID_TOTAL,(unsigned long)operation.errors);
    draw_text(sheep_view.window,20,146,line);
}
static int show_sheep(void) {
    if(sheep_view.window)return 1;
    if(!add_buttons(&sheep_view,0))return 0;
    if(!open_window(&sheep_view,"Dolly - Sheep Counter",
                   105,(ULONG)public_screen->BarHeight+1,
                   410,225)) {
        destroy_view(&sheep_view);
        return 0;
    }
    dolly_operation_show_sheep_counter(&operation);
    draw_sheep();
    return 1;
}
static void hide_sheep(void) {
    destroy_view(&sheep_view);
    dolly_operation_hide_sheep_counter(&operation);
}
static int process_view(DollyDemoView *v,int is_main,int *quit) {
    struct IntuiMessage *msg;
    if(!v->window)return 0;
    while((msg=GT_GetIMsg(v->window->UserPort))!=NULL) {
        ULONG cls=msg->Class;
        UWORD gadget_id=0;
        if(cls==IDCMP_GADGETUP && msg->IAddress)
            gadget_id=((struct Gadget *)msg->IAddress)->GadgetID;
        if(cls==IDCMP_REFRESHWINDOW) {
            GT_BeginRefresh(v->window);
            GT_EndRefresh(v->window,TRUE);
            if(is_main)draw_main(); else draw_sheep();
        }
        GT_ReplyIMsg(msg);
        if(is_main) {
            if(cls==IDCMP_CLOSEWINDOW || gadget_id==MAIN_QUIT_ID)
                *quit=1;
            else if(gadget_id==MAIN_SHOW_ID && !show_sheep())
                puts("MAIN WARN: cannot open Sheep Counter");
        } else if(cls==IDCMP_CLOSEWINDOW || gadget_id==SHEEP_HIDE_ID) {
            hide_sheep();
            return 1;
        }
    }
    return 0;
}
int main(void) {
    unsigned frames=0;
    int quit=0,rc=10;
    dolly_operation_init(&operation);
    if(dolly_operation_start(&operation,GRID_TOTAL)!=DOLLY_OP_OK)return 10;
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",36);
    if(!IntuitionBase)return 10;
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",36);
    GadToolsBase=OpenLibrary("gadtools.library",36);
    if(!GfxBase || !GadToolsBase)goto cleanup;
    public_screen=LockPubScreen(NULL);
    if(!public_screen)goto cleanup;
    if(public_screen->Width<640 || public_screen->Height<256)goto cleanup;
    draw_info=GetScreenDrawInfo(public_screen);
    if(!draw_info)goto cleanup;
    if(!add_buttons(&main_view,1))goto cleanup;
    if(!open_window(&main_view,"Dolly - Main window (demo)",14,
                    (ULONG)public_screen->BarHeight+14,390,190))
        goto cleanup;
    draw_main();
    if(!show_sheep())puts("MAIN WARN: Sheep Counter not opened");
    while(!quit) {
        WaitTOF();
        process_view(&main_view,1,&quit);
        if(sheep_view.window)process_view(&sheep_view,0,&quit);
        /* Progress belongs to the session, not either window. */
        if(++frames>=8u && operation.state==DOLLY_OP_RUNNING) {
            frames=0;
            if(operation.completed_blocks<GRID_TOTAL) {
                dolly_operation_progress(&operation,
                    operation.completed_blocks+1,
                    operation.errors,operation.retries);
                draw_main();
                if(sheep_view.window)draw_sheep();
            } else {
                dolly_operation_finish(&operation,1);
            }
        }
    }
    printf("SESSION blocks=%lu/%lu state=%u view_visible=%u\n",
           (unsigned long)operation.completed_blocks,
           (unsigned long)operation.total_blocks,
           (unsigned)operation.state,
           (unsigned)operation.sheep_counter_visible);
    rc=0;
cleanup:
    hide_sheep();
    destroy_view(&main_view);
    if(draw_info && public_screen)FreeScreenDrawInfo(public_screen,draw_info);
    if(public_screen)UnlockPubScreen(NULL,public_screen);
    if(GadToolsBase)CloseLibrary(GadToolsBase);
    if(GfxBase)CloseLibrary((struct Library *)GfxBase);
    if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
