/* Dolly M1.3: minimal 320x256 classic AmigaOS screen smoke.
 * AmigaOS 2.04+ / MC68000, no disk or device access.
 * This is NOT the production Sheep Counter or a GUI framework.
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

static void draw_line(struct RastPort *rp, LONG x, LONG y, const char *message) {
    SetAPen(rp, 1);
    Move(rp, x, y);
    Text(rp, (STRPTR)message, (ULONG)strlen(message));
}

int main(void) {
    struct Screen *screen = 0;
    ULONG free_before, free_open, free_after;
    ULONG largest_before, largest_open, largest_after;
    unsigned frame;
    int exit_code = 10;

    IntuitionBase = (struct IntuitionBase *)
        OpenLibrary("intuition.library", 36);
    if (!IntuitionBase) {
        puts("SCREEN FAIL: intuition.library V36 unavailable");
        return 10;
    }
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 36);
    if (!GfxBase) {
        puts("SCREEN FAIL: graphics.library V36 unavailable");
        goto cleanup;
    }

    free_before = AvailMem(MEMF_CHIP);
    largest_before = AvailMem(MEMF_CHIP | MEMF_LARGEST);
    printf("SCREEN before chip=%lu largest=%lu\n",
           (unsigned long)free_before, (unsigned long)largest_before);

    /* Decline if there is not even a conservative minimum of free RAM.
     * This is a guardrail, not a guarantee that opening the screen succeeds.
     */
    if (free_before < 131072ul || largest_before < 65536ul) {
        puts("SCREEN SKIP: insufficient free Chip RAM");
        exit_code = 20;
        goto cleanup;
    }

    screen = OpenScreenTags(NULL,
        SA_Left, 0,
        SA_Top, 0,
        SA_Width, 320,
        SA_Height, 256,
        SA_Depth, 2,
        SA_Type, CUSTOMSCREEN,
        SA_Title, (ULONG)"Dolly - Sheep Counter test",
        SA_ShowTitle, TRUE,
        TAG_DONE);
    if (!screen) {
        puts("SCREEN FAIL: OpenScreenTags returned NULL");
        goto cleanup;
    }

    ScreenToFront(screen);
    free_open = AvailMem(MEMF_CHIP);
    largest_open = AvailMem(MEMF_CHIP | MEMF_LARGEST);
    printf("SCREEN opened chip=%lu largest=%lu\n",
           (unsigned long)free_open, (unsigned long)largest_open);

    SetAPen(&screen->RastPort, 0);
    RectFill(&screen->RastPort, 0, 12, 319, 255);
    SetAPen(&screen->RastPort, 1);
    RectFill(&screen->RastPort, 8, 38, 311, 39);
    draw_line(&screen->RastPort, 18, 32, "DOLLY / SHEEP COUNTER");
    draw_line(&screen->RastPort, 18, 64, "MC68000  -  512K Chip RAM");
    draw_line(&screen->RastPort, 18, 92, "320 x 256 / 4 colours");
    draw_line(&screen->RastPort, 18, 125, "No disks are accessed.");
    draw_line(&screen->RastPort, 18, 155, "Baa! No sector left behind.");
    draw_line(&screen->RastPort, 18, 216, "Auto-close after ~5 seconds.");

    /* Bounded display: no gadget/event loop, no permanent screen lock.
     * WaitTOF is graphics.library; 250 PAL frames ~= 5 seconds.
     */
    for (frame = 0; frame < 250; ++frame) WaitTOF();

    CloseScreen(screen);
    screen = 0;
    free_after = AvailMem(MEMF_CHIP);
    largest_after = AvailMem(MEMF_CHIP | MEMF_LARGEST);
    printf("SCREEN closed chip=%lu largest=%lu\n",
           (unsigned long)free_after, (unsigned long)largest_after);
    printf("SCREEN %s\n",
           (free_after == free_before) ? "PASS" : "WARN: free RAM changed");
    exit_code = (free_after == free_before) ? 0 : 10;

cleanup:
    if (screen) CloseScreen(screen);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return exit_code;
}
