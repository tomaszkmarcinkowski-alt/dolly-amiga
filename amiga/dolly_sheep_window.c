/* M1.6: incremental Sheep Counter using classic GadTools + Intuition.
 * MC68000 / AmigaOS 2.04, only synthetic progress. No disk/device I/O.
 * Existing public screen is used; OS screen bar remains visible.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
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

#define COLS 20u
#define ROWS 6u
#define TOTAL (COLS * ROWS)
#define GID_CANCEL 100u
#define GID_BLOCKS 101u
#define GID_ERRORS 102u

typedef struct Ui {
    struct Screen *screen;     /* locked until a window is opened */
    struct Window *win;
    struct DrawInfo *draw_info;
    APTR visual;
    struct Gadget *gadgets;
    struct Gadget *blocks;
    struct Gadget *errors;
    UWORD pen_text, pen_back, pen_done, pen_shadow, pen_active;
    LONG gx, gy, progress_x, progress_y;
    unsigned done;
} Ui;

static void solid(struct RastPort *rp, UWORD pen,
                  LONG x1, LONG y1, LONG x2, LONG y2) {
    SetAPen(rp,pen);
    RectFill(rp,x1,y1,x2,y2);
}

static void draw_cell(Ui *ui, unsigned index, int active) {
    struct RastPort *rp=ui->win->RPort;
    LONG x=ui->gx+(LONG)(index%COLS)*26;
    LONG y=ui->gy+(LONG)(index/COLS)*19;
    solid(rp,ui->pen_shadow,x,y,x+23,y+16);
    solid(rp,(index<ui->done)?ui->pen_done:ui->pen_back,
          x+1,y+1,x+22,y+15);
    if(active) {
        /* Avoid extra bitmap/sprite: the ASCII marker is temporary Dolly. */
        solid(rp,ui->pen_active,x+2,y+2,x+21,y+14);
        SetAPen(rp,ui->pen_text);
        Move(rp,x+8,y+12);
        Text(rp,(STRPTR)"*",1);
    }
}

static void draw_initial(Ui *ui) {
    unsigned i;
    struct RastPort *rp=ui->win->RPort;
    LONG x=ui->gx, y=ui->gy;
    solid(rp,ui->pen_back,x,y-24,x+519,y-6);
    SetAPen(rp,ui->pen_text);
    Move(rp,x,y-10);
    Text(rp,(STRPTR)"Surface scan (synthetic, read-only)",35);
    for(i=0;i<TOTAL;++i)draw_cell(ui,i,i==ui->done);
    solid(rp,ui->pen_shadow,ui->progress_x,ui->progress_y,
          ui->progress_x+519,ui->progress_y+7);
    solid(rp,ui->pen_back,ui->progress_x+1,ui->progress_y+1,
          ui->progress_x+518,ui->progress_y+6);
}

static void advance(Ui *ui, unsigned next) {
    unsigned old=ui->done;
    LONG until;
    if(next>TOTAL)next=TOTAL;
    if(next<=old)return;
    /* Draw only the old and new current blocks, never the entire grid. */
    ui->done=next;
    draw_cell(ui,old,0);
    if(next<TOTAL)draw_cell(ui,next,1);
    until=ui->progress_x+(LONG)(next*518u/TOTAL);
    if(until>ui->progress_x) {
        solid(ui->win->RPort,ui->pen_done,
              ui->progress_x+1,ui->progress_y+1,until,ui->progress_y+6);
    }
    GT_SetGadgetAttrs(ui->blocks,ui->win,NULL,
                      GTNM_Number,(ULONG)next,TAG_DONE);
}

static int make_gadgets(Ui *ui, struct Screen *screen, ULONG window_top) {
    struct Gadget *previous;
    struct NewGadget ng;
    LONG y=(LONG)(screen->Height-window_top)-54;
    ui->visual=GetVisualInfo(screen,TAG_DONE);
    if(!ui->visual)return 0;
    previous=CreateContext(&ui->gadgets);
    if(!previous)return 0;
    memset(&ng,0,sizeof(ng));
    ng.ng_TextAttr=screen->Font;
    ng.ng_VisualInfo=ui->visual;
    ng.ng_Flags=0;
    ng.ng_LeftEdge=20; ng.ng_TopEdge=y;
    ng.ng_Width=148; ng.ng_Height=14;
    ng.ng_GadgetText=(STRPTR)"Blocks";
    ng.ng_GadgetID=GID_BLOCKS;
    previous=CreateGadget(NUMBER_KIND,previous,&ng,
                          GTNM_Number,0,GTNM_Border,TRUE,TAG_DONE);
    if(!previous)return 0;
    ui->blocks=previous;
    ng.ng_LeftEdge=240; ng.ng_GadgetText=(STRPTR)"Errors";
    ng.ng_GadgetID=GID_ERRORS;
    previous=CreateGadget(NUMBER_KIND,previous,&ng,
                          GTNM_Number,0,GTNM_Border,TRUE,TAG_DONE);
    if(!previous)return 0;
    ui->errors=previous;
    ng.ng_LeftEdge=(WORD)(screen->Width-111);
    ng.ng_Width=94; ng.ng_Height=16;
    ng.ng_GadgetText=(STRPTR)"Cancel";
    ng.ng_GadgetID=GID_CANCEL;
    previous=CreateGadget(BUTTON_KIND,previous,&ng,TAG_DONE);
    return previous!=NULL;
}

int main(void) {
    Ui ui;
    ULONG before,opened,after;
    ULONG top,height;
    unsigned tick=0, quit=0;
    int rc=10;
    memset(&ui,0,sizeof(ui));
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",36);
    if(!IntuitionBase) {puts("WINDOW FAIL: intuition V36");return rc;}
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",36);
    if(!GfxBase) {puts("WINDOW FAIL: graphics V36");goto cleanup;}
    GadToolsBase=OpenLibrary("gadtools.library",36);
    if(!GadToolsBase) {puts("WINDOW FAIL: gadtools V36");goto cleanup;}
    ui.screen=LockPubScreen(NULL);
    if(!ui.screen) {puts("WINDOW SKIP: public screen unavailable");rc=20;goto cleanup;}
    if(ui.screen->Width<640 || ui.screen->Height<256) {
        puts("WINDOW SKIP: requires 640x256 public screen");
        rc=20;goto cleanup;
    }
    top=(ULONG)ui.screen->BarHeight;
    height=(ULONG)ui.screen->Height-top;
    if(!make_gadgets(&ui,ui.screen,top)) {
        puts("WINDOW FAIL: GadTools creation");goto cleanup;
    }
    ui.draw_info=GetScreenDrawInfo(ui.screen);
    if(!ui.draw_info) {puts("WINDOW FAIL: DrawInfo");goto cleanup;}
    ui.pen_text=ui.draw_info->dri_Pens[TEXTPEN];
    ui.pen_back=ui.draw_info->dri_Pens[BACKGROUNDPEN];
    ui.pen_shadow=ui.draw_info->dri_Pens[SHADOWPEN];
    ui.pen_done=ui.draw_info->dri_Pens[FILLPEN];
    ui.pen_active=ui.draw_info->dri_Pens[SHINEPEN];
    before=AvailMem(MEMF_CHIP);
    printf("WINDOW before chip=%lu\n",(unsigned long)before);
    if(before<131072ul || AvailMem(MEMF_CHIP|MEMF_LARGEST)<65536ul) {
        puts("WINDOW SKIP: low Chip RAM");rc=20;goto cleanup;
    }
    ui.win=OpenWindowTags(NULL,
        WA_PubScreen,(ULONG)ui.screen,
        WA_Title,(ULONG)"Dolly - Sheep Counter (GadTools)",
        WA_Left,0,WA_Top,top,
        WA_Width,(ULONG)ui.screen->Width,
        WA_Height,height,
        WA_Gadgets,(ULONG)ui.gadgets,
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,
        WA_CloseGadget,TRUE,WA_Activate,TRUE,
        WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP,
        TAG_DONE);
    if(!ui.win) {puts("WINDOW FAIL: OpenWindowTags");goto cleanup;}
    UnlockPubScreen(NULL,ui.screen);
    ui.screen=NULL;
    /* These coordinates are relative to the window and its borders.
     * This test layout is intentionally restricted to 640x256+.
     */
    ui.gx=(LONG)ui.win->BorderLeft+14;
    ui.gy=(LONG)ui.win->BorderTop+42;
    ui.progress_x=ui.gx;
    ui.progress_y=ui.gy+119;
    GT_RefreshWindow(ui.win,NULL);
    draw_initial(&ui);
    opened=AvailMem(MEMF_CHIP);
    printf("WINDOW open chip=%lu\n",(unsigned long)opened);
    while(!quit && ui.done<TOTAL) {
        struct IntuiMessage *msg;
        WaitTOF();
        while((msg=GT_GetIMsg(ui.win->UserPort))!=NULL) {
            if(msg->Class==IDCMP_CLOSEWINDOW)quit=1;
            if(msg->Class==IDCMP_GADGETUP) {
                struct Gadget *g=(struct Gadget *)msg->IAddress;
                if(g && g->GadgetID==GID_CANCEL)quit=1;
            }
            GT_ReplyIMsg(msg);
        }
        if(++tick==5u) {
            tick=0;
            advance(&ui,ui.done+1u);
        }
    }
    /* A small bounded hold avoids losing the end state instantly. */
    for(tick=0;tick<25u && !quit;++tick)WaitTOF();
    FreeScreenDrawInfo(ui.win->WScreen,ui.draw_info);
    ui.draw_info=NULL;
    CloseWindow(ui.win);ui.win=NULL;
    after=AvailMem(MEMF_CHIP);
    printf("WINDOW closed chip=%lu\n",(unsigned long)after);
    puts(before==after ? "WINDOW PASS: Chip RAM restored" :
                       "WINDOW WARN: Chip RAM changed");
    rc=before==after ? 0 : 10;
cleanup:
    if(ui.draw_info && ui.win)FreeScreenDrawInfo(ui.win->WScreen,ui.draw_info);
    else if(ui.draw_info && ui.screen)FreeScreenDrawInfo(ui.screen,ui.draw_info);
    if(ui.win)CloseWindow(ui.win);
    if(ui.gadgets)FreeGadgets(ui.gadgets);
    if(ui.visual)FreeVisualInfo(ui.visual);
    if(ui.screen)UnlockPubScreen(NULL,ui.screen);
    if(GadToolsBase)CloseLibrary(GadToolsBase);
    if(GfxBase)CloseLibrary((struct Library *)GfxBase);
    if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
