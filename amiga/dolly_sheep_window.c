/* M1.5: Workbench/public-screen Sheep Counter window smoke.
 * MC68000 / AmigaOS 2.04, synthetic progress only, no disk I/O.
 * Default uses existing public screen and its pens/font.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <graphics/gfxbase.h>
#include <graphics/rastport.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <stdio.h>
#include <string.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static void fill(struct RastPort *rp, UWORD pen,
                 LONG x1, LONG y1, LONG x2, LONG y2) {
    SetAPen(rp, pen);
    RectFill(rp, x1, y1, x2, y2);
}
static void caption(struct RastPort *rp, UWORD pen,
                    LONG x, LONG y, const char *s) {
    SetAPen(rp, pen);
    Move(rp, x, y);
    Text(rp, (STRPTR)s, (ULONG)strlen(s));
}
static void draw(struct Window *win, struct DrawInfo *di, unsigned step) {
    struct RastPort *rp = win->RPort;
    UWORD text = di->dri_Pens[TEXTPEN];
    UWORD background = di->dri_Pens[BACKGROUNDPEN];
    UWORD highlight = di->dri_Pens[SHINEPEN];
    UWORD shadow = di->dri_Pens[SHADOWPEN];
    UWORD filled = di->dri_Pens[FILLPEN];
    LONG x = (LONG)win->BorderLeft + 12;
    LONG y = (LONG)win->BorderTop + 12;
    LONG content_right = (LONG)win->Width - (LONG)win->BorderRight - 12;
    LONG content_bottom = (LONG)win->Height - (LONG)win->BorderBottom - 10;
    LONG grid_x = x;
    LONG grid_y = y + 28;
    unsigned row, col;
    char status[88];
    unsigned done = step * 6u;
    fill(rp, background, x, y, content_right, content_bottom);
    caption(rp, text, x, y+9, "Surface scan - synthetic demonstration (read only)");
    /* 20 columns, 6 rows; no offscreen bitmap or extra screen. */
    for (row=0; row<6u; ++row) {
        for (col=0; col<20u; ++col) {
            LONG bx = grid_x+(LONG)col*26;
            LONG by = grid_y+(LONG)row*19;
            unsigned index = row*20u+col;
            UWORD pen = index < done ? filled : background;
            fill(rp, shadow, bx, by, bx+23, by+16);
            fill(rp, pen, bx+1, by+1, bx+22, by+15);
            if (index==done && step<20u) {
                fill(rp, highlight, bx, by, bx+23, by+16);
                /* The ASCII star is Dolly's temporary current-block marker. */
                caption(rp, text, bx+8, by+12, "*");
            }
        }
    }
    fill(rp, shadow, x, y+149, grid_x+519, y+156);
    if (done) fill(rp, filled, x, y+149, x+(LONG)(done*520u/120u)-1, y+156);
    sprintf(status, "Blocks: %u/120   Errors: 0   Retries: 0", done);
    caption(rp, text, x, y+176, status);
    caption(rp, text, x, y+192, "Dolly: one sheep, zero disk operations.");
}
int main(void) {
    struct Screen *pub = NULL;
    struct Window *win = NULL;
    struct DrawInfo *di = NULL;
    ULONG before, after, during;
    ULONG window_top, window_height;
    unsigned step, tick;
    int rc = 10, quit = 0;
    IntuitionBase=(struct IntuitionBase *)OpenLibrary("intuition.library", 36);
    if(!IntuitionBase) {puts("WINDOW FAIL: Intuition V36 required");return rc;}
    GfxBase=(struct GfxBase *)OpenLibrary("graphics.library", 36);
    if(!GfxBase) {puts("WINDOW FAIL: graphics V36 required");goto cleanup;}
    pub=LockPubScreen(NULL);
    if(!pub) {puts("WINDOW SKIP: no available public screen");rc=20;goto cleanup;}
    if(pub->Width < 640 || pub->Height < 256) {
        puts("WINDOW SKIP: needs at least 640x256 public screen");
        rc=20;goto cleanup;
    }
    di=GetScreenDrawInfo(pub);
    if(!di) {puts("WINDOW FAIL: GetScreenDrawInfo");goto cleanup;}
    before=AvailMem(MEMF_CHIP);
    printf("WINDOW before chip=%lu\n",(unsigned long)before);
    if(before < 131072ul || AvailMem(MEMF_CHIP|MEMF_LARGEST) < 65536ul) {
        puts("WINDOW SKIP: low Chip RAM");rc=20;goto cleanup;
    }
    /* Fill all space below the system screen bar; never cover the bar.
     * WA_Width/Height refer to the complete window including its borders.
     */
    window_top=(ULONG)pub->BarHeight;
    window_height=(ULONG)pub->Height-window_top;
    win=OpenWindowTags(NULL,
        WA_PubScreen, (ULONG)pub,
        WA_Title, (ULONG)"Dolly - Sheep Counter",
        WA_Left, 0, WA_Top, window_top,
        WA_Width, (ULONG)pub->Width, WA_Height, window_height,
        WA_DragBar, TRUE, WA_DepthGadget, TRUE,
        WA_CloseGadget, TRUE, WA_Activate, TRUE,
        WA_SmartRefresh, TRUE,
        WA_IDCMP, IDCMP_CLOSEWINDOW,
        TAG_DONE);
    if(!win) {puts("WINDOW FAIL: OpenWindowTags");goto cleanup;}
    /* The window owns a reference to the public screen now. */
    UnlockPubScreen(NULL,pub); pub=NULL;
    during=AvailMem(MEMF_CHIP);
    printf("WINDOW open chip=%lu\n",(unsigned long)during);
    for(step=0;step<=20u && !quit;++step) {
        struct IntuiMessage *msg;
        draw(win, di, step);
        for(tick=0;tick<12u;++tick) {
            WaitTOF();
            while((msg=(struct IntuiMessage *)GetMsg(win->UserPort))!=NULL) {
                if(msg->Class==IDCMP_CLOSEWINDOW) quit=1;
                ReplyMsg((struct Message *)msg);
            }
            if(quit)break;
        }
    }
    FreeScreenDrawInfo(win->WScreen,di);di=NULL;
    CloseWindow(win);win=NULL;
    after=AvailMem(MEMF_CHIP);
    printf("WINDOW closed chip=%lu\n",(unsigned long)after);
    puts(before==after ? "WINDOW PASS: Chip RAM restored" :
                       "WINDOW WARN: Chip RAM changed");
    rc=before==after ? 0 : 10;
cleanup:
    if(win)CloseWindow(win);
    if(di && win)FreeScreenDrawInfo(win->WScreen,di);
    else if(di && pub)FreeScreenDrawInfo(pub,di);
    if(pub)UnlockPubScreen(NULL,pub);
    if(GfxBase)CloseLibrary((struct Library *)GfxBase);
    if(IntuitionBase)CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
