/* Dolly M1.4: synthetic Sheep Counter screen demo, 68000 + AmigaOS 2.04.
 * Four-colour 320x256 Intuition screen, no file/device I/O, no audio.
 * Temporary procedural checkerboard sheep; not final artwork or GUI.
 */
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <graphics/gfxbase.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <stdio.h>
#include <string.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

static void fill(struct RastPort *rp, UBYTE pen,
                 LONG x1, LONG y1, LONG x2, LONG y2) {
    SetAPen(rp, pen);
    RectFill(rp, x1, y1, x2, y2);
}
static void label(struct RastPort *rp, UBYTE pen, LONG x, LONG y,
                  const char *value) {
    SetAPen(rp, pen);
    Move(rp, x, y);
    Text(rp, (STRPTR)value, (ULONG)strlen(value));
}
static void sheep(struct RastPort *rp, LONG x, LONG y) {
    unsigned row, col;
    /* One inexpensive placeholder sheep body, 6x4 checkerboard tiles. */
    fill(rp, 3, x+5, y+1, x+43, y+27);
    for (row=0; row<4; ++row) {
        for (col=0; col<6; ++col) {
            fill(rp, ((row+col)&1u) ? 2 : 1,
                 x+7+(LONG)col*6, y+3+(LONG)row*6,
                 x+12+(LONG)col*6, y+8+(LONG)row*6);
        }
    }
    fill(rp, 3, x+44, y+9, x+53, y+22);
    fill(rp, 1, x+51, y+12, x+53, y+14);
    fill(rp, 3, x+12, y+28, x+16, y+34);
    fill(rp, 3, x+36, y+28, x+40, y+34);
    fill(rp, 3, x+1, y+7, x+5, y+12);
}
static void render(struct RastPort *rp, unsigned step) {
    unsigned col, row;
    char detail[64];
    const unsigned total = 12u*6u;
    const unsigned done = step*6u;
    /* Three horizontal zones: heading, 12x6 block grid, status. */
    fill(rp, 0, 0, 0, 319, 255);
    fill(rp, 2, 0, 0, 319, 19);
    label(rp, 1, 8, 13, "DOLLY  /  SHEEP COUNTER");
    label(rp, 1, 8, 36, "Synthetic Scandisk progress");
    for (row=0; row<6; ++row) {
        for (col=0; col<12; ++col) {
            LONG left=9+(LONG)col*25;
            LONG top=48+(LONG)row*24;
            UBYTE pen = (col < step) ? 3 : 1;
            fill(rp, pen, left, top, left+22, top+21);
            fill(rp, 0, left+2, top+2, left+20, top+19);
            if (col < step) fill(rp, 3, left+3, top+3, left+19, top+18);
        }
    }
    /* Sheep moves with the current column. The demo never reads a disk. */
    sheep(rp, 9+(LONG)((step < 12u ? step : 12u)*20u), 103);
    fill(rp, 3, 8, 202, 311, 207);
    if (step) fill(rp, 1, 8, 202, 8+(LONG)(step*25u), 207);
    sprintf(detail, "Blocks: %u/%u   Errors: 0", done, total);
    label(rp, 1, 9, 225, detail);
    fill(rp, 2, 0, 232, 319, 255);
    label(rp, 1, 8, 247,
          step == 12u ? "Baa! All synthetic blocks OK." :
                        "MC68000 | 512K | Baa in progress");
}
int main(void) {
    static const UWORD colours[4] = {0x012, 0xfff, 0xd22, 0x888};
    struct Screen *screen = NULL;
    ULONG before, opened, after, largest;
    unsigned step, tick;
    int rc = 10;
    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library",36);
    if (!IntuitionBase) {
        puts("SHEEP FAIL: intuition.library V36 unavailable");
        return rc;
    }
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library",36);
    if (!GfxBase) {
        puts("SHEEP FAIL: graphics.library V36 unavailable");
        goto cleanup;
    }
    before = AvailMem(MEMF_CHIP);
    largest = AvailMem(MEMF_CHIP | MEMF_LARGEST);
    printf("SHEEP before chip=%lu largest=%lu\n",
           (unsigned long)before, (unsigned long)largest);
    if (before < 131072ul || largest < 65536ul) {
        puts("SHEEP SKIP: not enough free Chip RAM");
        rc = 20;
        goto cleanup;
    }
    screen = OpenScreenTags(NULL,
        SA_Left, 0, SA_Top, 0,
        SA_Width, 320, SA_Height, 256,
        SA_Depth, 2, SA_Type, CUSTOMSCREEN,
        SA_Title, (ULONG)"Dolly Sheep Counter - DEMO",
        SA_ShowTitle, FALSE, TAG_DONE);
    if (!screen) {
        puts("SHEEP FAIL: OpenScreenTags");
        goto cleanup;
    }
    ScreenToFront(screen);
    LoadRGB4(&screen->ViewPort, colours, 4);
    opened = AvailMem(MEMF_CHIP);
    printf("SHEEP open chip=%lu\n", (unsigned long)opened);
    for (step=0; step<=12; ++step) {
        render(&screen->RastPort, step);
        for (tick=0; tick<20u; ++tick) WaitTOF();
    }
    CloseScreen(screen);
    screen = NULL;
    after = AvailMem(MEMF_CHIP);
    printf("SHEEP closed chip=%lu\n", (unsigned long)after);
    printf("SHEEP %s\n", before==after ? "PASS" : "WARN: chip changed");
    rc = before==after ? 0 : 10;
cleanup:
    if (screen) CloseScreen(screen);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
