/* M1.7: resizable GadTools / Intuition Sheep Counter on a public screen.
 * Hide or the close gadget dismisses ONLY this view. In the standalone
 * synthetic demo that ends the simulation; real work will be core-owned.
 * Classic MC68000, AmigaOS 2.04+, synthetic progress; no device I/O.
 * Redraw all only on resize/refresh, not on each progress update.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <graphics/text.h>
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
#define GID_HIDE 100u
#define GID_BLOCKS 101u
#define GID_ERRORS 102u
#define MIN_WIDTH 420u
#define MIN_HEIGHT 205u

typedef struct Layout {
    LONG gx,gy,stepx,stepy,progress_x,progress_y,progress_w;
    LONG number_y,button_y,button_x,button_w,number_w;
    LONG left,right,bottom;
} Layout;
typedef struct Ui {
    struct Screen *screen;
    struct Window *win;
    struct DrawInfo *di;
    APTR visual;
    struct Gadget *gadgets,*blocks,*errors;
    UWORD text,back,done,shade,active;
    Layout l;
    unsigned done_blocks;
} Ui;
static void rect(struct RastPort *rp,UWORD pen,
                 LONG x1,LONG y1,LONG x2,LONG y2) {
    if(x2<x1 || y2<y1)return;
    SetAPen(rp,pen);RectFill(rp,x1,y1,x2,y2);
}
static void write_text(struct RastPort *rp,UWORD pen,LONG x,LONG baseline,
                       const char *value) {
    SetAPen(rp,pen);Move(rp,x,baseline);
    Text(rp,(STRPTR)value,(ULONG)strlen(value));
}
static int calculate(Ui *u) {
    Layout *l=&u->l;
    LONG inner_w,inner_h,top,bottom,grid_height;
    struct Window *w=u->win;
    inner_w=(LONG)w->Width-w->BorderLeft-w->BorderRight;
    inner_h=(LONG)w->Height-w->BorderTop-w->BorderBottom;
    if(inner_w<400 || inner_h<155)return 0;
    l->left=w->BorderLeft+10;
    l->right=(LONG)w->Width-w->BorderRight-10;
    l->bottom=(LONG)w->Height-w->BorderBottom-6;
    l->button_w=88;
    l->button_x=l->right-l->button_w;
    l->button_y=l->bottom-16;
    l->number_y=l->button_y;
    l->number_w=(l->button_x-l->left-32)/2;
    l->progress_x=l->left;
    l->progress_w=l->right-l->left;
    l->progress_y=l->button_y-27;
    top=w->BorderTop+26;
    bottom=l->progress_y-12;
    grid_height=bottom-top;
    if(grid_height<(LONG)(ROWS*10u))return 0;
    l->gx=l->left;
    l->gy=top;
    l->stepx=l->progress_w/(LONG)COLS;
    l->stepy=grid_height/(LONG)ROWS;
    if(l->stepx<10 || l->stepy<10)return 0;
    return 1;
}
static void draw_cell(Ui *u,unsigned idx,int active) {
    Layout *l=&u->l;
    struct RastPort *rp=u->win->RPort;
    LONG x=l->gx+(LONG)(idx%COLS)*l->stepx;
    LONG y=l->gy+(LONG)(idx/COLS)*l->stepy;
    LONG x2=x+l->stepx-3,y2=y+l->stepy-3;
    rect(rp,u->shade,x,y,x2,y2);
    rect(rp,idx<u->done_blocks?u->done:u->back,x+1,y+1,x2-1,y2-1);
    if(active) {
        struct TextFont *font=rp->Font;
        LONG star_w=(LONG)TextLength(rp,(STRPTR)"*",1);
        LONG sx=x+(l->stepx-star_w)/2;
        LONG sy=y+(l->stepy-(LONG)font->tf_YSize)/2+
                (LONG)font->tf_Baseline;
        rect(rp,u->active,x+2,y+2,x2-2,y2-2);
        write_text(rp,u->text,sx,sy,"*");
    }
}
static void draw_all(Ui *u) {
    Layout *l=&u->l;
    struct RastPort *rp=u->win->RPort;
    unsigned i;
    LONG bar_end;
    rect(rp,u->back,u->win->BorderLeft,u->win->BorderTop,
         (LONG)u->win->Width-u->win->BorderRight-1,
         l->progress_y-2);
    write_text(rp,u->text,l->left,u->win->BorderTop+16,
               "Surface scan (synthetic, read-only)");
    for(i=0;i<TOTAL;++i)draw_cell(u,i,i==u->done_blocks && i<TOTAL);
    rect(rp,u->shade,l->progress_x,l->progress_y,
         l->progress_x+l->progress_w-1,l->progress_y+8);
    rect(rp,u->back,l->progress_x+1,l->progress_y+1,
         l->progress_x+l->progress_w-2,l->progress_y+7);
    bar_end=l->progress_x+1+
          (LONG)(u->done_blocks*(unsigned long)(l->progress_w-2)/TOTAL);
    if(bar_end>l->progress_x+1)
        rect(rp,u->done,l->progress_x+1,l->progress_y+1,
             bar_end-1,l->progress_y+7);
    /* Labels sit above NUMBER_KIND gadget rectangles, never outside window. */
    write_text(rp,u->text,l->left,l->number_y-5,"Blocks:");
    write_text(rp,u->text,l->left+l->number_w+12,l->number_y-5,"Errors:");
}
static void advance(Ui *u,unsigned next) {
    unsigned old=u->done_blocks;
    Layout *l=&u->l;
    LONG oldend,newend;
    if(next>TOTAL)next=TOTAL;
    if(next<=old)return;
    oldend=l->progress_x+1+
        (LONG)(old*(unsigned long)(l->progress_w-2)/TOTAL);
    u->done_blocks=next;
    draw_cell(u,old,0);
    if(next<TOTAL)draw_cell(u,next,1);
    newend=l->progress_x+1+
        (LONG)(next*(unsigned long)(l->progress_w-2)/TOTAL);
    if(newend>oldend)
        rect(u->win->RPort,u->done,oldend,l->progress_y+1,
             newend-1,l->progress_y+7);
    GT_SetGadgetAttrs(u->blocks,u->win,NULL,
                      GTNM_Number,(ULONG)next,TAG_DONE);
}
static int make_gadgets(Ui *u) {
    struct Gadget *prev;
    struct NewGadget ng;
    Layout *l=&u->l;
    prev=CreateContext(&u->gadgets);
    if(!prev)return 0;
    memset(&ng,0,sizeof(ng));
    ng.ng_VisualInfo=u->visual;
    ng.ng_TextAttr=u->win->WScreen->Font;
    ng.ng_LeftEdge=(WORD)l->left;
    ng.ng_TopEdge=(WORD)l->number_y;
    ng.ng_Width=(WORD)l->number_w;
    ng.ng_Height=16;
    ng.ng_GadgetText=(STRPTR)"";
    ng.ng_GadgetID=GID_BLOCKS;
    prev=CreateGadget(NUMBER_KIND,prev,&ng,
        GTNM_Number,(ULONG)u->done_blocks,GTNM_Border,TRUE,TAG_DONE);
    if(!prev)return 0;
    u->blocks=prev;
    ng.ng_LeftEdge=(WORD)(l->left+l->number_w+12);
    ng.ng_GadgetID=GID_ERRORS;
    prev=CreateGadget(NUMBER_KIND,prev,&ng,
        GTNM_Number,0,GTNM_Border,TRUE,TAG_DONE);
    if(!prev)return 0;
    u->errors=prev;
    ng.ng_LeftEdge=(WORD)l->button_x;
    ng.ng_TopEdge=(WORD)l->button_y;
    ng.ng_Width=(WORD)l->button_w;
    ng.ng_GadgetID=GID_HIDE;
    ng.ng_GadgetText=(STRPTR)"Hide";
    prev=CreateGadget(BUTTON_KIND,prev,&ng,TAG_DONE);
    return prev!=NULL;
}
static int replace_gadgets(Ui *u) {
    if(u->gadgets) {
        RemoveGList(u->win,u->gadgets,-1);
        FreeGadgets(u->gadgets);
        u->gadgets=NULL;u->blocks=NULL;u->errors=NULL;
    }
    if(!calculate(u))return 0;
    /* Clear the old layout BEFORE attaching GadTools widgets; never paint
     * over gadget imagery after GT_RefreshWindow().
     */
    rect(u->win->RPort,u->back,
         (LONG)u->win->BorderLeft,(LONG)u->win->BorderTop,
         (LONG)u->win->Width-u->win->BorderRight-1,
         (LONG)u->win->Height-u->win->BorderBottom-1);
    if(!make_gadgets(u))return 0;
    AddGList(u->win,u->gadgets,(ULONG)-1,-1,NULL);
    RefreshGList(u->gadgets,u->win,NULL,-1);
    GT_RefreshWindow(u->win,NULL);
    return 1;
}
int main(void) {
    Ui u;
    ULONG before,opened,after,top,height;
    unsigned tick=0,hide_requested=0,relayout=0,redraw=0;
    int rc=10;
    memset(&u,0,sizeof(u));
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library",36);
    if(!IntuitionBase){puts("WINDOW FAIL: Intuition V36");return rc;}
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library",36);
    if(!GfxBase){puts("WINDOW FAIL: graphics V36");goto cleanup;}
    GadToolsBase=OpenLibrary("gadtools.library",36);
    if(!GadToolsBase){puts("WINDOW FAIL: gadtools V36");goto cleanup;}
    u.screen=LockPubScreen(NULL);
    if(!u.screen){puts("WINDOW SKIP: public screen unavailable");rc=20;goto cleanup;}
    if((ULONG)u.screen->Width<MIN_WIDTH ||
       (ULONG)u.screen->Height<MIN_HEIGHT+(ULONG)u.screen->BarHeight) {
        puts("WINDOW SKIP: public screen too small");rc=20;goto cleanup;
    }
    top=(ULONG)u.screen->BarHeight;
    height=(ULONG)u.screen->Height-top;
    u.visual=GetVisualInfo(u.screen,TAG_DONE);
    u.di=GetScreenDrawInfo(u.screen);
    if(!u.visual || !u.di){puts("WINDOW FAIL: system visual info");goto cleanup;}
    u.text=u.di->dri_Pens[TEXTPEN];
    u.back=u.di->dri_Pens[BACKGROUNDPEN];
    u.shade=u.di->dri_Pens[SHADOWPEN];
    u.done=u.di->dri_Pens[FILLPEN];
    u.active=u.di->dri_Pens[SHINEPEN];
    before=AvailMem(MEMF_CHIP);
    printf("WINDOW before chip=%lu\n",(unsigned long)before);
    if(before<131072ul || AvailMem(MEMF_CHIP|MEMF_LARGEST)<65536ul){
        puts("WINDOW SKIP: low Chip RAM");rc=20;goto cleanup;
    }
    u.win=OpenWindowTags(NULL,
        WA_PubScreen,(ULONG)u.screen,
        WA_Title,(ULONG)"Dolly - Sheep Counter (GadTools)",
        WA_Left,0,WA_Top,top,
        WA_Width,(ULONG)u.screen->Width,WA_Height,height,
        WA_MinWidth,MIN_WIDTH,WA_MinHeight,MIN_HEIGHT,
        WA_MaxWidth,(ULONG)u.screen->Width,
        WA_MaxHeight,height,
        WA_DragBar,TRUE,WA_DepthGadget,TRUE,
        WA_CloseGadget,TRUE,WA_SizeGadget,TRUE,
        WA_Activate,TRUE,WA_SmartRefresh,TRUE,
        WA_IDCMP,IDCMP_CLOSEWINDOW|IDCMP_GADGETUP|
                 IDCMP_NEWSIZE|IDCMP_REFRESHWINDOW,
        TAG_DONE);
    if(!u.win){puts("WINDOW FAIL: OpenWindowTags");goto cleanup;}
    UnlockPubScreen(NULL,u.screen);u.screen=NULL;
    if(!replace_gadgets(&u)){puts("WINDOW FAIL: initial GadTools");goto cleanup;}
    draw_all(&u);
    opened=AvailMem(MEMF_CHIP);
    printf("WINDOW open chip=%lu\n",(unsigned long)opened);
    while(!hide_requested && u.done_blocks<TOTAL) {
        struct IntuiMessage *msg;
        WaitTOF();
        while((msg=GT_GetIMsg(u.win->UserPort))!=NULL) {
            ULONG cls=msg->Class;
            if(cls==IDCMP_CLOSEWINDOW)hide_requested=1;
            if(cls==IDCMP_GADGETUP) {
                struct Gadget *g=(struct Gadget *)msg->IAddress;
                if(g && g->GadgetID==GID_HIDE)hide_requested=1;
            }
            if(cls==IDCMP_NEWSIZE)relayout=1;
            if(cls==IDCMP_REFRESHWINDOW) {
                GT_BeginRefresh(u.win);
                GT_EndRefresh(u.win,TRUE);
                redraw=1;
            }
            GT_ReplyIMsg(msg);
        }
        if(relayout) {
            relayout=0;
            if(!replace_gadgets(&u)){
                puts("WINDOW FAIL: resize reflow");
                rc=10;goto cleanup;
            }
            redraw=1;
        }
        if(redraw) {
            redraw=0;
            draw_all(&u);
        }
        if(++tick==5u) {
            tick=0;
            advance(&u,u.done_blocks+1u);
        }
    }
    for(tick=0;tick<25u && !hide_requested;++tick)WaitTOF();
    FreeScreenDrawInfo(u.win->WScreen,u.di);u.di=NULL;
    CloseWindow(u.win);u.win=NULL;
    after=AvailMem(MEMF_CHIP);
    printf("WINDOW closed chip=%lu\n",(unsigned long)after);
    if(hide_requested)
        puts("WINDOW HIDDEN: demonstration view closed (no disk operation exists)");
    puts(before==after ? "WINDOW PASS: Chip RAM restored" :
                       "WINDOW WARN: Chip RAM changed");
    rc=before==after?0:10;
cleanup:
    if(u.win) {
        if(u.gadgets)RemoveGList(u.win,u.gadgets,-1);
        if(u.di) {
            FreeScreenDrawInfo(u.win->WScreen,u.di);
            u.di=NULL;
        }
        CloseWindow(u.win);
    }
    if(u.gadgets)FreeGadgets(u.gadgets);
    if(u.di && u.screen)FreeScreenDrawInfo(u.screen,u.di);
    if(u.visual)FreeVisualInfo(u.visual);
    if(u.screen)UnlockPubScreen(NULL,u.screen);
    if(GadToolsBase)CloseLibrary(GadToolsBase);
    if(GfxBase)CloseLibrary((struct Library *)GfxBase);
    if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
